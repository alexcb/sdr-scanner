squelch = 200
audio_squelch = 0
wait = 2000
for freq in range(144000000, 148000000, 5000):
    print(f'{freq}     {squelch}       {audio_squelch}   {wait}     {wait}')
