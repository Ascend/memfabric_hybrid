## C API for general things

[TOC]

## 1 Version

### 1.1 Get version of library

#### Functionality description

Get version of this library, e.g. 0.1.0.
Format: {major_version}.{minor_version}.{fix}

#### Function definition

```c
const char *zbal_version()
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                   |
|-------------------|--------|-------------------------------|
| return            |        | string of version, e.g. 0.1.0 |

## 2 Logger

### 2.1 Set external log function

#### Functionality description

In order to put all log messages in united way, this library allows user to set external log function to define custom
logger. In the log function, user can generate the messages to log file etc.
Without set the external log function, this library generate the log messages to stdout.

#### Function definition

```c
int32_t zbal_set_logger(void (*func)(int, const char *))
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description           |
|-------------------|--------|-----------------------|
| func              | in     | external log function |
| return            |        | 0 if successful       |

### 2.2 Set log level

#### Functionality description

Set the log level of this library.
It supports the following levels: debug | info | warn | error.

#### Function definition

```c
int32_t zbal_set_logger_level(int level)
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                              |
|-------------------|--------|------------------------------------------|
| level             | in     | log level, 0:debug 1:info 2:warn 3:error |
| return            |        | 0 if successful                          |

## 3 Last error message

### 3.1 Get last error message

#### Functionality description

If there is an error happens, user can get the error message using this function.

#### Function definition

```c
const char* zbal_get_last_error_msg()
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                  |
|-------------------|--------|------------------------------|
| return            |        | string of last error message |                         |

### 3.2 Get and clear last error message

#### Functionality description

If there is an error happens, user can get the error message by this function, and also clear the last error message
after this function called.

#### Function definition

```c
const char* zbal_get_and_clear_last_error_msg()
```

#### Description of parameters and return value

| Parameters/return | In/Out | Description                  |
|-------------------|--------|------------------------------|
| return            |        | string of last error message |
