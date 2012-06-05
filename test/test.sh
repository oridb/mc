#!/bin/bash
export PATH=.:$PATH

function prints {
    if [ `./$1` -ne $2 ]; then
        echo "FAIL: $1"
    else
        echo "PASS: $1"
    fi
}

function returns {
    ./$1
    if [ $? -eq $2 ]; then
        echo "PASS: $1"
    else
        echo "FAIL: $1"
    fi
}

returns main 0
returns add 53
returns struct 42
