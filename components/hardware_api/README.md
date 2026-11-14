Future component. This MVP exposes no ordinary GPIO/UART/I2C handles. Such
permissions are rejected by the manifest validator. Add resource ownership,
reserved-pin checks, bounded I/O and termination cleanup before exposing them.
