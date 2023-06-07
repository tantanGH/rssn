#ifndef __H_UART__
#define __H_UART__

#define UART_BUFFER_SIZE (64*1024)

#define UART_OK       (0)
#define UART_EXIT     (1)
#define UART_QUIT     (2)
#define UART_TIMEOUT  (3)

typedef struct {

  int16_t tmsio;
  int16_t rsdrv;

  int32_t baud_rate;
  int32_t timeout;

  uint8_t* buffer;
  size_t buffer_size;

  uint8_t* buffer_original;
  size_t buffer_size_original;

} UART;

int32_t uart_open(UART* uart, int32_t baud_rate, int32_t timeout);
void uart_close(UART* uart);
int32_t uart_write(UART* uart, uint8_t* buffer, size_t len);
int32_t uart_read(UART* uart, uint8_t* buffer, size_t len);

#endif