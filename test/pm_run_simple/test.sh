#!/bin/bash

grep "in fetch_and_rm_cache call start" log | wc -l
grep "TW: in fetch_and_rm_cache call before create lock" log | wc -l
grep "TW: in fetch_and_rm_cache call after create lock" log | wc -l
grep "TW: in fetch_and_rm_cache call, outer if with" log
grep "TW: in fetch_and_rm_cache call, outer if with" log | wc -l
grep "TW: in fetch_and_rm_cache call first stage finished," log | wc -l
grep "TW: in fetch_and_rm_cache call second stage finished" log | wc -l
grep "TW: in fetch_and_rm_cache call, third stage, p1" log | wc -l
grep "TW: in fetch_and_rm_cache call four stage finished" log | wc -l
