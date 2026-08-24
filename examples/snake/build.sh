#!/bin/bash

set -e

dry snake.dry out.g2
g2a out.g2 out.g2b
cg2 out.g2b -s 10