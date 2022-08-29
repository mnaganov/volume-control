#!/bin/sh

datastore="http://localhost:1280/0001f2fffe00447b/datastore"
master_trim="ext/obank/0/ch/0/stereoTrim"
slave_trim="ext/obank/2/ch/__chan__/trim"
slave_chans="0 1 2 3 4"
data_str=""

for chan in $slave_chans; do
    if [ $data_str ]; then
        data_str="$data_str,"
    fi
    slave_name=`echo $slave_trim | sed -n "s/__chan__/$chan/p"`
    data_str="$data_str\"$slave_name\":__trim__"
done
#echo $data_str

master_value=""
until [ $master_value ]; do
    master_value=`curl -s $datastore/$master_trim | sed -n "s/^.*\"value\"\:\([-0-9]*\).*/\1/p"`
done
echo master trim $master_value

data_values=`echo $data_str | sed -n "s/__trim__/$master_value/gp"`
#echo $data_values
curl --data "json={$data_values}" $datastore

while true; do
    last_etag=""
    until [ $last_etag ]; do
        last_etag=`curl -s -D - $datastore -o /dev/null | grep ETag: | sed -n "s/^.*\ \([0-9]*\)/\1/p"`
    done
    #echo $last_etag

    master_value=""
    until [ $master_value ]; do
        master_value=`curl -s -H "If-None-Match:$last_etag" $datastore/$master_trim | sed -n "s/^.*\"value\"\:\([-0-9]*\).*/\1/p"`
    done
    echo master trim $master_value

    data_values=`echo $data_str | sed -n "s/__trim__/$master_value/gp"`
    curl --data "json={$data_values}" $datastore
done
