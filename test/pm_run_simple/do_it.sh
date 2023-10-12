#!/bin/bash

rm log_*; ./interactive_run_cpu.sh 2>&1 | tee log
