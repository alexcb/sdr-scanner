#pragma once

struct radio_scanner;

int init_radio( struct radio_scanner** rs, int freq );
int stop_radio( void );
int set_radio_volume( struct radio_scanner* rs, int volume );
