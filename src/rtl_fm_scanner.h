//#pragma once
//
struct radio_scanner;

typedef void (*dbms_cb_t)(double *dbms, int start_freq, int freq_step, int num_steps, void *user_data);


//
int init_radio(struct radio_scanner **rs, dbms_cb_t dbms_cb, void *user_data);
int stop_radio( struct radio_scanner* rs );
//int set_radio_volume( struct radio_scanner* rs, int volume );
//int set_radio_freq( struct radio_scanner* rs, int freq );
//int get_radio_signal_strength( struct radio_scanner* rs );
//int get_radio_raw_buf( struct radio_scanner* rs, unsigned char *buf );
//int get_radio_rot_buf( struct radio_scanner* rs, unsigned char *buf );
//int get_radio_lowpassed( struct radio_scanner* rs, int16_t *buf );
//
int radio_sample( struct radio_scanner* rs, int freq_low, int freq_high );
