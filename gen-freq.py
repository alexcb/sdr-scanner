#!/usr/bin/env python3
squelch = 250
audio_squelch = 80
wait = 100000

# repeater outputs
for freq in range(145100000, 145500000, 5000):
    print(f'{freq}     {squelch}       {audio_squelch}   {wait}     {wait}')

for freq in range(146610000, 147390000, 5000):
    print(f'{freq}     {squelch}       {audio_squelch}   {wait}     {wait}')

for freq in range(442000000, 445000000, 12500):
    print(f'{freq}     {squelch}       {audio_squelch}   {wait}     {wait}')

# simplex
for freq in range(146400000, 146580000, 5000):
    print(f'{freq}     {squelch}       {audio_squelch}   {wait}     {wait}')
for freq in range(147420000, 147570000, 5000):
    print(f'{freq}     {squelch}       {audio_squelch}   {wait}     {wait}')
for freq in range(446000000, 446175000, 12500):
    print(f'{freq}     {squelch}       {audio_squelch}   {wait}     {wait}')
