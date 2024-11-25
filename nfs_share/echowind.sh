#!/bin/bash
echo 1 > /sys/class/gpio/gpio914/value
sleep 3
echo 0 > /sys/class/gpio/gpio914/value
