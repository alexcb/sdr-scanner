#!/usr/bin/env python3
vhf_squelch = 700
uhf_squelch = 100
audio_squelch = 80
wait = 100000

vhf_spacing = 30000
uhf_spacing = 30000

# repeater outputs
for freq in range(145100000, 145500000, vhf_spacing):
    print(f'{freq}     {vhf_squelch}       {audio_squelch}   {wait}     {wait}')

for freq in range(146610000, 147390000, vhf_spacing):
    print(f'{freq}     {vhf_squelch}       {audio_squelch}   {wait}     {wait}')

for freq in range(442000000, 445000000, uhf_spacing):
    print(f'{freq}     {uhf_squelch}       {audio_squelch}   {wait}     {wait}')

# simplex
for freq in range(146400000, 146580000, vhf_spacing):
    print(f'{freq}     {vhf_squelch}       {audio_squelch}   {wait}     {wait}')
for freq in range(147420000, 147570000, vhf_spacing):
    print(f'{freq}     {vhf_squelch}       {audio_squelch}   {wait}     {wait}')
for freq in range(446000000, 446175000, uhf_spacing):
    print(f'{freq}     {uhf_squelch}       {audio_squelch}   {wait}     {wait}')
