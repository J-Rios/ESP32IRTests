/**************************************************************************************************/
/* Name:                                                                                          */
/*     ESP32IRTests                                                                               */
/* Description:                                                                                   */
/*     Receive and send IR signals and codes from an ESP32.                                       */
/* Creation Date:                                                                                 */
/*     22/03/2020                                                                                 */
/* Last modified date:                                                                            */
/*     22/03/2020                                                                                 */
/**************************************************************************************************/

/* Libraries */

#include <IRremote.h>
#include "ircodes.h"

/**************************************************************************************************/

/* Defines */

// Receive and Transmit pins
#define PIN_I_IR_RX 14
#define PIN_O_IR_TX 12

// Serial Port communication speed bauds
#define SERIAL_BAUDS 115200

// Serial Reception buffer size (Maximum length for each received line)
#define RX_BUFFER_SIZE 512

/**************************************************************************************************/

/* Functions Prototypes */

// Check for incomming Serial data, store it in provided buffer and detect end of line
int8_t serial_line_received(char* my_rx_buffer, uint16_t* rx_buffer_received_bytes, 
    const size_t rx_buffer_max_size);

// Send a NEC code message
void ir_send_nec(const uint16_t code);

// Send an Overflow NEC message
void ir_send_nec_ovf(void);

// Display IR code
void ircode(decode_results *results);

// Display encoding type
void encoding(decode_results *results);

// Dump out the decode_results structure
void dump_info(decode_results *results);

// Dump out the decode_results structure
void dump_code(decode_results *results);

// Convert string into unsigned 8 bytes value
int8_t cstr_string_to_u8(char* str_in, size_t str_in_len, uint8_t* value_out, uint8_t base);

// Convert string into unsigned 16 bytes value
int8_t cstr_string_to_u16(char* str_in, size_t str_in_len, uint16_t* value_out, uint8_t base);

// Convert string into unsigned 32 bytes value
int8_t cstr_string_to_u32(char* str_in, size_t str_in_len, uint32_t* value_out, uint8_t base);

/**************************************************************************************************/

/* Data Types */

// Functions Return Codes
enum _return_codes
{
    RC_OK = 0,
    RC_BAD = -1,
    RC_INVALID_INPUT = -2,
    RC_CUSTOM_DELAY = 100
};

/**************************************************************************************************/

/* Global Objects */

IRrecv ir_rx(PIN_I_IR_RX);
IRsend ir_tx(PIN_O_IR_TX);

/**************************************************************************************************/

/* Setup and Loop Functions */

void setup(void)
{
    // Initialize the software serial port
    Serial.println("UART Serial initializing...");
    Serial.begin(SERIAL_BAUDS);

    Serial.println("Starting IR receiver...");
    ir_rx.enableIRIn();

    Serial.println("Setup done.\n");
}

void loop(void)
{
    // Serial Rx buffer and number of received bytes in the buffer
    static char _rx_buffer[RX_BUFFER_SIZE] = { 0 };
    static uint16_t _rx_buffer_received_bytes = 0;

    // IR Mode (0 - None, 1 - Receive Mode, 2 - Transmit mode)
    static uint8_t ir_mode = 1;

    // IR code variable
    uint16_t code = 0;

    // Received IR signal data
    decode_results ir_rx_data;

    // Check for incomming Serial data lines
    if(serial_line_received(_rx_buffer, &_rx_buffer_received_bytes, RX_BUFFER_SIZE) == RC_OK)
    {
        Serial.print("> ");
        Serial.println(_rx_buffer);

        if(strncmp(_rx_buffer, "help\0", strlen("help\0")) == 0)
        {
            Serial.println("Avaliable commands:");
            Serial.println("ir mode rx - Change to IR receive signals mode");
            Serial.println("ir mode tx - Change to IR transmit signals mode");
            _rx_buffer_received_bytes = 0;
            return;
        }
        else if(strncmp(_rx_buffer, "ir mode rx\0", strlen("ir mode rx\0")) == 0)
        {
            if(ir_mode != 1)
            {
                Serial.println("Changed to IR Receive mode.");
                ir_mode = 1;
            }
            _rx_buffer_received_bytes = 0;
            return;
        }
        else if(strncmp(_rx_buffer, "ir mode tx\0", strlen("ir mode tx\0")) == 0)
        {
            if(ir_mode != 2)
            {
                Serial.println("Changed to IR Transmit mode.");
                Serial.println("Send IR NEC codes, for example:");
                Serial.println("0x10EF\n");
                ir_mode = 2;
            }
            _rx_buffer_received_bytes = 0;
            return;
        }

        // IR Transmit Mode
        if(ir_mode == 2)
        {
            if(cstr_string_to_u16(_rx_buffer, _rx_buffer_received_bytes, &code, 16) != RC_OK)
                return;

            ir_send_nec(code);
            Serial.print("Sent code: 0x");
            Serial.println(code, HEX);
        }

        _rx_buffer_received_bytes = 0;
    }

    // IR Receive Mode
    if(ir_mode == 1)
    {
        if(ir_rx.decode(&ir_rx_data))
        {
            dump_info(&ir_rx_data);
            dump_code(&ir_rx_data);
            Serial.println("");
            ir_rx.resume();
        }
    }
}

/**************************************************************************************************/

/* Serial Line Received Detector Function */

// Check for incomming Serial data, store it in provided buffer and detect end of line
int8_t serial_line_received(char* my_rx_buffer, uint16_t* rx_buffer_received_bytes, 
    const size_t rx_buffer_max_size)
{
    uint16_t i = *rx_buffer_received_bytes;

    if(i > rx_buffer_max_size-1)
        return RC_INVALID_INPUT;

    // While there is any data incoming from the hardware serial port
    while(Serial.available())
    {
        my_rx_buffer[i] = (char)Serial.read();

        i = i + 1;
        *rx_buffer_received_bytes = i;

        if(i >= rx_buffer_max_size-1)
        {
            my_rx_buffer[rx_buffer_max_size-1] = '\0';
            return RC_OK;
        }

        if((my_rx_buffer[i-1] == '\n') || (my_rx_buffer[i-1] == '\r'))
        {
            my_rx_buffer[i-1] = '\0';
            *rx_buffer_received_bytes = *rx_buffer_received_bytes - 1;
            return RC_OK;
        }
    }

    return RC_BAD;
}

/**************************************************************************************************/

/* IR Functions */

// Send a NEC code message
void ir_send_nec(const uint16_t code)
{
    ir_tx.sendNEC(NEC_INIT_MASK | code, 32);
}

// Send an Overflow NEC message
void ir_send_nec_ovf(void)
{
    ir_tx.sendNEC(NEC_OVERFLOW, 32);
}

// Display IR code
void ircode(decode_results *results)
{
    // Panasonic has an Address
    if (results->decode_type == PANASONIC)
    {
        Serial.print(results->address, HEX);
        Serial.print(":");
    }

    // Print Code
    Serial.print(results->value, HEX);
}

// Display encoding type
void encoding(decode_results *results)
{
    switch (results->decode_type)
    {
        default:
        case UNKNOWN:      Serial.print("UNKNOWN");      break;
        case NEC:          Serial.print("NEC");          break;
        case SONY:         Serial.print("SONY");         break;
        case RC5:          Serial.print("RC5");          break;
        case RC6:          Serial.print("RC6");          break;
        case DISH:         Serial.print("DISH");         break;
        case SHARP:        Serial.print("SHARP");        break;
        case JVC:          Serial.print("JVC");          break;
        case SANYO:        Serial.print("SANYO");        break;
        case MITSUBISHI:   Serial.print("MITSUBISHI");   break;
        case SAMSUNG:      Serial.print("SAMSUNG");      break;
        case LG:           Serial.print("LG");           break;
        case WHYNTER:      Serial.print("WHYNTER");      break;
        case AIWA_RC_T501: Serial.print("AIWA_RC_T501"); break;
        case PANASONIC:    Serial.print("PANASONIC");    break;
        case DENON:        Serial.print("Denon");        break;
    }
}

// Dump out the decode_results structure
void dump_info(decode_results *results)
{
    // Check if the buffer overflowed
    if (results->overflow)
    {
        Serial.println("IR code too long. Edit IRremoteInt.h and increase RAWBUF");
        return;
    }

    // Show Encoding standard
    Serial.print("Encoding  : "); encoding(results);
    Serial.println("");

    // Show Code & length
    Serial.print("Code      : "); ircode(results);
    Serial.print(" ("); Serial.print(results->bits, DEC); Serial.println(" bits)");
}

// Dump out the decode_results structure
void dump_code(decode_results *results)
{
    Serial.print("uint16_t  "); Serial.print("rawData[");
    Serial.print(results->rawlen - 1, DEC); Serial.print("] = {");
    for (int i = 1;  i < results->rawlen;  i++)
    {
        Serial.print(results->rawbuf[i] * USECPERTICK, DEC);
        if(i < results->rawlen-1)
            Serial.print(",");
        if(!(i & 1))
            Serial.print(" ");
    }
    Serial.print("};");

    // Comment
    Serial.print("  // "); encoding(results);
    Serial.print(" "); ircode(results);
    Serial.println("");

    // Now dump "known" codes
    if (results->decode_type != UNKNOWN)
    {
        // Some protocols have an address
        if (results->decode_type == PANASONIC)
        {
            Serial.print("uint16_t addr = 0x");
            Serial.print(results->address, HEX);
            Serial.println(";");
        }

        // All protocols have data
        Serial.print("uint16_t data = 0x");
        Serial.print(results->value, HEX);
        Serial.println(";");
    }
}

/**************************************************************************************************/

/* Auxiliar Functions */

// Convert string into unsigned 8 bytes value
int8_t cstr_string_to_u8(char* str_in, size_t str_in_len, uint8_t* value_out, uint8_t base)
{
    return cstr_string_to_u32(str_in, str_in_len, (uint32_t*)value_out, base);
}

// Convert string into unsigned 16 bytes value
int8_t cstr_string_to_u16(char* str_in, size_t str_in_len, uint16_t* value_out, uint8_t base)
{
    return cstr_string_to_u32(str_in, str_in_len, (uint32_t*)value_out, base);
}

// Convert string into unsigned 32 bytes value
int8_t cstr_string_to_u32(char* str_in, size_t str_in_len, uint32_t* value_out, uint8_t base)
{
    char* ptr = str_in;
	uint8_t digit;

	*value_out = 0;

    // Check for hexadecimal "0x" bytes and go through it
    if(base == 16)
    {
        if((ptr[1] == 'x') || (ptr[1] == 'X'))
        {
            if(str_in_len < 3)
                return RC_INVALID_INPUT;
            ptr = ptr + 2;
            str_in_len = str_in_len - 2;
        }
    }

    // Convert each byte of the string
    for(uint16_t i = 0; i < str_in_len; i++)
    {
        if(base == 10)
        {
            if(ptr[i] >= '0' && ptr[i] <= '9')
			    digit = ptr[i] - '0';
            else
                return RC_INVALID_INPUT;
        }
        else if(base == 16)
        {
            if(ptr[i] >= '0' && ptr[i] <= '9')
                digit = ptr[i] - '0';
            else if (base == 16 && ptr[i] >= 'a' && ptr[i] <= 'f')
                digit = ptr[i] - 'a' + 10;
            else if (base == 16 && ptr[i] >= 'A' && ptr[i] <= 'F')
                digit = ptr[i] - 'A' + 10;
            else
                return RC_INVALID_INPUT;
        }
        else
			return RC_INVALID_INPUT;

		*value_out = ((*value_out)*base) + digit;
	}

	return RC_OK;
}

