#!/bin/bash

if [ ! -f $1 ] ; then
    echo "$0: Cannot find file $1"
    exit 0
fi


# This script pulls data out of the map file that is created by the
# linker, calculates the locations and sizes of the text, data, bss,
# and stack sections, and reports the maximim heap size.

printf "\nSection\t\tStart\t\tEnd\t\tSize\n"
printf "==========================================================\n"

TextStart=`grep __Begin_Text__ $1 | cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
TextEnd=`grep __End_Text__ $1 | cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
printf "Text:\t\t0x%X\t\t0x%X\t\t0x%X\n" $TextStart $((TextEnd - 1)) $((TextEnd - TextStart))

DataStart=`grep __Begin_Data__ $1 | grep -v - | cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
DataEnd=`grep __End_Data__ $1 | grep -v ASSERT | cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
printf "Data (runtime):\t0x%X\t0x%X\t0x%X\n" $DataStart $((DataEnd - 1)) $((DataEnd - DataStart))

BssStart=`grep __End_Data__ $1 | cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
BssEnd=`grep __End_Bss__ $1 | grep -v ASSERT | cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
printf "BSS:\t\t0x%X\t0x%X\t0x%X\n" $BssStart $((BssEnd - 1)) $((BssEnd - BssStart))

StackStart=`grep __StackLimit $1 | grep -v ASSERT | grep -v \- | cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
StackEnd=`grep __StackTop $1 | grep -v '!provide'| cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
printf "STACK:\t\t0x%X\t0x%X\t0x%X\n" $StackStart $((StackEnd - 1)) $((StackEnd - StackStart))

HeapTop=`grep HeapTop $1 | grep -v ASSERT | cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
HeapBase=`grep HeapBase $1 | grep -v ASSERT | cut -f 1 | sed 's/ \+/\t/g' | cut -f 2`
printf "\nMaximum heap size: 0x%X\n\n" $((HeapTop - HeapBase + 1))
