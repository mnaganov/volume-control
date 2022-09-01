#!/bin/sh

datastore="http://localhost:1280/0001f2fffe00447b/datastore"
master_trim="ext/obank/0/ch/0/stereoTrim"
slave_trim="ext/obank/2/ch/__chan__/trim"
slave_chans="0 1 2 3 4"
slave_data_str=""
mute_trim="ext/obank/2/ch/5/trim"
master_and_mute_data_str="\"$master_trim\":__master_trim__,\"$mute_trim\":__mute_trim__"
sound_on_button="ext/ibank/0/ch/1/pad"
poll_subtree="ext"

for chan in $slave_chans; do
    if [ $slave_data_str ]; then
        slave_data_str="$slave_data_str,"
    fi
    slave_name=`echo $slave_trim | sed -n "s/__chan__/$chan/p"`
    slave_data_str="$slave_data_str\"$slave_name\":__master_trim__"
done
#echo $slave_data_str

while true; do

master_value=""
while [ -z "$master_value" ]; do
    master_value=`curl -s $datastore/$master_trim | sed -n "s/^.*\"value\"\:\([-0-9]*\).*/\1/p"`
done
mute_value=""
while [ -z "$mute_value" ]; do
    mute_value=`curl -s $datastore/$mute_trim | sed -n "s/^.*\"value\"\:\([-0-9]*\).*/\1/p"`
done
sound_on_button_value=""
while [ -z "$sound_on_button_value" ]; do
    sound_on_button_value=`curl -s $datastore/$sound_on_button | sed -n "s/^.*\"value\"\:\([0-9]*\).*/\1/p"`
done

swap=0
if [ "$sound_on_button_value" -eq "1" ]; then
    if [ "$mute_value" -gt "$master_value" ]; then
        swap=1
    fi
else
    if [ "$master_value" -gt "$mute_value" ]; then
        swap=1
    fi
fi
if [ "$swap" -eq "1" ]; then
    temp=$mute_value
    mute_value=$master_value
    master_value=$temp
fi
echo master trim $master_value, mute trim $mute_value

data_values=`echo $master_and_mute_data_str,$slave_data_str | sed -n "s/__master_trim__/$master_value/gp" | sed -n "s/__mute_trim__/$mute_value/p"`
#echo $data_values
curl --data "json={$data_values}" $datastore

last_etag=""
while [ -z "$last_etag" ]; do
    last_etag=`curl -s -D - $datastore -o /dev/null | grep ETag: | sed -n "s/^.*\ \([0-9]*\)/\1/p"`
done
#echo $last_etag

poll_value=""
while [ -z "$poll_value" ]; do
    # Match '{' to ensure that the reply is not empty
    poll_value=`curl -s -H "If-None-Match:$last_etag" $datastore/$poll_subtree | sed -n "s/^.*\([{]\).*/\1/p"`
    #echo "$poll_value"
done

done # while true
