#!/usr/bin/env bash
let week=7*24*60*60
for filename in $(find $1 -type l)
do
	if [ -h "$filename" ] && [ ! -e "$filename" ] && [ $(($(date +%s) - $(stat "$filename" -c %Y))) -gt $week ];
	then echo "$filename"; 
	fi
done