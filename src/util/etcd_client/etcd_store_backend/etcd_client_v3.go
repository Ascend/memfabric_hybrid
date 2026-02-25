/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * MemFabric_Hybrid is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
package main

/*
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Opaque pointer definition
typedef struct EtcdClient EtcdClient;
*/
import "C"

import (
	"context"
	"fmt"
	"os"
	"runtime/cgo"
	"strings"
	"sync"
	"time"
	"unsafe"

	clientv3 "go.etcd.io/etcd/client/v3"
	"go.etcd.io/etcd/client/v3/concurrency"
)

// -----------------------------------------------------------------------------
// Constants & Configuration
// -----------------------------------------------------------------------------

const (
	DefaultLockKey = "/huawei/memfabric_hybrid/etcd_client/global_lock"

	// DefaultOpTimeout defines the default timeout for internal etcd operations
	// if the client-level timeout is too loose.
	DefaultOpTimeout = 5 * time.Second

	// SessionTTL defines the TTL for distributed lock session.
	// If process dies, lock is released after this duration.
	SessionTTL = 10

	// CleanupTimeout defines the timeout for cleanup operations during Close.
	CleanupTimeout = 2 * time.Second
)

// -----------------------------------------------------------------------------
// Struct Definitions
// -----------------------------------------------------------------------------

// etcdWrapper encapsulates the etcd client and its state.
// It guarantees thread-safety for C callers.
type etcdWrapper struct {
	client  *clientv3.Client
	timeout time.Duration

	// mu protects mutable fields (lastErrC, session, mutex)
	mu sync.Mutex

	// lastErrC holds the C-allocated string of the last error.
	// Contract: Owned by Go, freed by Go before update or on close.
	// Read-only for C callers via Etcd_GetLastError.
	lastErrC *C.char

	// Concurrency primitives for distributed lock
	session *concurrency.Session
	mutex   *concurrency.Mutex
}

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

// setError updates the error state safely.
// Caller must NOT hold w.mu when calling this method.
func (w *etcdWrapper) setError(err error) {
	w.mu.Lock()
	defer w.mu.Unlock()
	w.setErrorLocked(err)
}

// setErrorLocked updates the error state.
// Caller MUST hold w.mu when calling this method.
func (w *etcdWrapper) setErrorLocked(err error) {
	if w.lastErrC != nil {
		C.free(unsafe.Pointer(w.lastErrC))
		w.lastErrC = nil
	}

	if err != nil {
		w.lastErrC = C.CString(err.Error())
	}
}

// getWrapper retrieves the Go struct from the C handle safely.
func getWrapper(ptr *C.EtcdClient) *etcdWrapper {
	if ptr == nil {
		return nil
	}
	defer func() {
		if r := recover(); r != nil {
			fmt.Fprintf(os.Stderr, "[etcd-cgo] getWrapper panic recovered: %v\n", r)
		}
	}()

	h := cgo.Handle(uintptr(unsafe.Pointer(ptr)))
	if v, ok := h.Value().(*etcdWrapper); ok {
		return v
	}
	return nil
}

// drainKeepAlive consumes the keepalive channel to prevent blocking.
// It runs until the context is canceled or the channel closes.
func drainKeepAlive(ctx context.Context, ch <-chan *clientv3.LeaseKeepAliveResponse) {
	if ch == nil {
		return
	}
	for {
		select {
		case <-ctx.Done():
			return
		case resp, ok := <-ch:
			if !ok {
				return
			}
			if resp == nil {
				fmt.Fprintf(os.Stderr, "[etcd-cgo] lease keepalive lost\n")
			}
		}
	}
}

// -----------------------------------------------------------------------------
// Exported C API
// -----------------------------------------------------------------------------

//export Etcd_New
func Etcd_New(endpoints *C.char, username *C.char, password *C.char, timeoutSeconds C.int) *C.EtcdClient {
	if endpoints == nil {
		return nil
	}

	goEndpoints := C.GoString(endpoints)
	goUsername := ""
	if username != nil {
		goUsername = C.GoString(username)
	}
	goPassword := ""
	if password != nil {
		goPassword = C.GoString(password)
	}

	eps := strings.Split(goEndpoints, ",")
	dur := time.Duration(timeoutSeconds) * time.Second
	if dur <= 0 {
		dur = DefaultOpTimeout
	}

	config := clientv3.Config{
		Endpoints:            eps,
		DialTimeout:          dur,
		Username:             goUsername,
		Password:             goPassword,
		PermitWithoutStream:  true,
		DialKeepAliveTime:    10 * time.Second,
		DialKeepAliveTimeout: 3 * time.Second,
		AutoSyncInterval:     time.Minute,
	}

	cli, err := clientv3.New(config)
	if err != nil {
		fmt.Fprintf(os.Stderr, "[etcd-cgo] Etcd_New failed: %v\n", err)
		return nil
	}

	wrapper := &etcdWrapper{
		client:  cli,
		timeout: dur,
	}

	h := cgo.NewHandle(wrapper)
	return (*C.EtcdClient)(unsafe.Pointer(uintptr(h)))
}

//export Etcd_Close
func Etcd_Close(client *C.EtcdClient) {
	if client == nil {
		return
	}

	h := cgo.Handle(uintptr(unsafe.Pointer(client)))
	wrapper, ok := h.Value().(*etcdWrapper)
	if !ok {
		return
	}
	wrapper.mu.Lock()
	lastErr := wrapper.lastErrC
	wrapper.lastErrC = nil
	session := wrapper.session
	mutex := wrapper.mutex
	wrapper.session = nil
	wrapper.mutex = nil
	cli := wrapper.client
	wrapper.client = nil
	wrapper.mu.Unlock()
	if lastErr != nil {
		C.free(unsafe.Pointer(lastErr))
	}

	if mutex != nil {
		ctx, cancel := context.WithTimeout(context.Background(), CleanupTimeout)
		_ = mutex.Unlock(ctx)
		cancel()
	}

	if session != nil {
		_ = session.Close()
	}

	if cli != nil {
		_ = cli.Close()
	}

	h.Delete()
}

//export Etcd_GetLastError
func Etcd_GetLastError(client *C.EtcdClient) *C.char {
	w := getWrapper(client)
	if w == nil {
		return nil
	}

	w.mu.Lock()
	defer w.mu.Unlock()
	return w.lastErrC
}

//export Etcd_Put
func Etcd_Put(client *C.EtcdClient, key *C.char, value unsafe.Pointer, valueLen C.size_t, ttlSeconds C.int64_t) C.int {
	w := getWrapper(client)
	if w == nil {
		return -1
	}

	if key == nil {
		w.setError(fmt.Errorf("key is nil"))
		return -1
	}

	goKey := C.GoString(key)
	var valStr string
	if value != nil && valueLen > 0 {
		valStr = string(unsafe.Slice((*byte)(value), int(valueLen)))
	}

	ctx, cancel := context.WithTimeout(context.Background(), w.timeout)
	defer cancel()

	var err error
	var opts []clientv3.OpOption

	if ttlSeconds > 0 {
		var leaseResp *clientv3.LeaseGrantResponse
		leaseResp, err = w.client.Grant(ctx, int64(ttlSeconds))
		if err != nil {
			w.setError(fmt.Errorf("lease grant failed: %w", err))
			return -1
		}

		kaCh, kaErr := w.client.KeepAlive(context.Background(), leaseResp.ID)
		if kaErr != nil {
			fmt.Fprintf(os.Stderr, "[etcd-cgo] KeepAlive failed for lease %x: %v\n", leaseResp.ID, kaErr)
		} else {
			go drainKeepAlive(w.client.Ctx(), kaCh)
		}

		opts = append(opts, clientv3.WithLease(leaseResp.ID))
	}

	_, err = w.client.Put(ctx, goKey, valStr, opts...)
	if err != nil {
		w.setError(err)
		return -1
	}

	w.setError(nil)
	return 0
}

//export Etcd_Get
func Etcd_Get(client *C.EtcdClient, key *C.char, outValue **C.char, outValueLen *C.size_t) C.int {
	w := getWrapper(client)
	if w == nil {
		return -1
	}

	if key == nil {
		w.setError(fmt.Errorf("key is nil"))
		return -1
	}

	if outValue == nil || outValueLen == nil {
		w.setError(fmt.Errorf("output parameters are nil"))
		return -1
	}

	goKey := C.GoString(key)
	ctx, cancel := context.WithTimeout(context.Background(), w.timeout)
	defer cancel()

	resp, err := w.client.Get(ctx, goKey)
	if err != nil {
		w.setError(err)
		return -1
	}

	if len(resp.Kvs) == 0 {
		w.setError(fmt.Errorf("key not found"))
		return -1
	}

	valBytes := resp.Kvs[0].Value
	length := len(valBytes)

	if length == 0 {
		cBuf := (*C.char)(C.malloc(1))
		if cBuf == nil {
			w.setError(fmt.Errorf("malloc failed"))
			return -1
		}
		*cBuf = 0
		*outValue = cBuf
		*outValueLen = 0
	} else {
		*outValue = (*C.char)(C.CBytes(valBytes))
		*outValueLen = C.size_t(length)
	}

	w.setError(nil)
	return 0
}

//export Etcd_FreeValue
func Etcd_FreeValue(value *C.char) {
	if value != nil {
		C.free(unsafe.Pointer(value))
	}
}

//export Etcd_Remove
func Etcd_Remove(client *C.EtcdClient, key *C.char) C.int {
	w := getWrapper(client)
	if w == nil {
		return -1
	}

	if key == nil {
		w.setError(fmt.Errorf("key is nil"))
		return -1
	}

	goKey := C.GoString(key)
	ctx, cancel := context.WithTimeout(context.Background(), w.timeout)
	defer cancel()

	_, err := w.client.Delete(ctx, goKey)
	if err != nil {
		w.setError(err)
		return -1
	}

	w.setError(nil)
	return 0
}

//export Etcd_Lock
func Etcd_Lock(client *C.EtcdClient) C.int {
	w := getWrapper(client)
	if w == nil {
		return -1
	}

	w.mu.Lock()

	if w.mutex != nil {
		w.setErrorLocked(fmt.Errorf("lock already held by this client"))
		w.mu.Unlock()
		return -1
	}

	if w.session == nil {
		s, err := concurrency.NewSession(w.client, concurrency.WithTTL(SessionTTL))
		if err != nil {
			w.setErrorLocked(fmt.Errorf("session init failed: %w", err))
			w.mu.Unlock()
			return -1
		}
		w.session = s
	}

	mutex := concurrency.NewMutex(w.session, DefaultLockKey)
	w.mutex = mutex
	w.mu.Unlock()
	err := mutex.Lock(context.Background())
	if err != nil {
		w.setError(err)

		// Clean up on failure
		w.mu.Lock()
		if w.session != nil {
			_ = w.session.Close()
			w.session = nil
		}
		w.mutex = nil
		w.mu.Unlock()
		return -1
	}

	w.setError(nil)
	return 0
}

//export Etcd_UnLock
func Etcd_UnLock(client *C.EtcdClient) C.int {
	w := getWrapper(client)
	if w == nil {
		return -1
	}

	w.mu.Lock()
	mutex := w.mutex
	if mutex == nil {
		w.setErrorLocked(fmt.Errorf("lock not held"))
		w.mu.Unlock()
		return -1
	}

	w.mutex = nil
	w.mu.Unlock()

	ctx, cancel := context.WithTimeout(context.Background(), w.timeout)
	err := mutex.Unlock(ctx)
	cancel()

	if err != nil {
		w.setError(err)
		return -1
	}

	w.setError(nil)
	return 0
}

func main() {}