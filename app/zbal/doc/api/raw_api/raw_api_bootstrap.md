## C API of bootstrap

[TOC]

### 1 Options

### 1.1 Initialize the options struct of bootstrap

#### Functionality description

Initialize the struct and set the default values, with this user only need to change the value of options what need to
be set.

#### Function definition

```c
int32_t zbal_bootstrap_options_init(zbal_bootstrap_options_t *options)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                        |
| ----------------- | ------ | ---------------------------------- |
| options           | in     | options ptr need to be initialized |
| return            |        | 0 if successful                    |

### 2 Bootstrap and Un-bootstrap

### 2.1 Bootstrap

#### Functionality description

Bootstrap this library major things done in this function: 1) GVA will be created, 2) Bootstrap state is set, 3) Memory
space for meta is reserved. User also get inner state via output.

#### Function definition

```c
int32_t zbal_bootstrap(zbal_bootstrap_options_t *options, zbal_bootstrap_output_t* output)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                        |
| ----------------- | ------ | ---------------------------------- |
| options           | in     | options ptr need to be initialized |
| output            | out    | output after bootstrap             |
| return            |        | 0 if successful                    |

### 2.2 Un-bootstrap

#### Functionality description

Un-bootstrap this library major things done in this function: 1) Communicators will be destroyed, 2) GVA will be
destroyed.

#### Function definition

```c
int32_t zbal_unbootstrap(uint32_t flags)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description     |
| ----------------- | ------ | --------------- |
| flags             | in     | reserved flags  |
| return            |        | 0 if successful |