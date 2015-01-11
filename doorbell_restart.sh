#!/bin/bash

gksudo killall -9 doorbell_start.sh
cd /home/sergey/Develop/Doorbell/host
gksudo ./main
