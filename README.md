# multi-runner
Orchestration of a batch processing computing system written using C syscalls

## Building
`make all`

## Usage
First, start up the controller with
`./bin/controller <max-parallel> <sched-policy>`

Then, you can submit each task with
`./bin/runner -e <user-id> "<program> <arg1> <arg2> ..."`

`./bin/runner -s` will shutdown the controller and `./bin/runner -c` will show the current status of your jobs

