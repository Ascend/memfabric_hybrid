#ifndef SHM_ALL_REDUCE_H
#define SHM_ALL_REDUCE_H

void shm_all_reduce_do(uint32_t coreDim, void* stream, uint8_t* gva, uint64_t spaceOffset,
    uint64_t flagOffset, uint32_t rankId, uint32_t rankSize);

#endif // SHM_ALL_REDUCE_H