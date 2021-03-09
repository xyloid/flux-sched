package main

import (
        . "fluxcli"
        "fmt"
        "io/ioutil"
        "flag"
)

func main() {
        ctx := NewReapiCli()
        jgfPtr := flag.String("jgf", "", "path to jgf")
        jobspecPtr := flag.String("jobspec", "", "path to jobspec")
        reserve := flag.Bool("reserve", false, "or else reserve?")
        flag.Parse()

        jgf, err1 := ioutil.ReadFile(*jgfPtr)
        err2 := ReapiCliInit(ctx, string(jgf))
        jobspec, err3 := ioutil.ReadFile(*jobspecPtr)
        fmt.Printf("Jobspec:\n %s\n", jobspec)

        reserved, allocated, at, overhead, jobid, err4  := ReapiCliMatchAllocate(ctx, *reserve, string(jobspec))
        fmt.Printf("Match Allocate output:\n %s, %t, %d, %f, %s, %d, %s, %d, %d\n", allocated, reserved, at, overhead, err1, err2, err3, err4, jobid)
        reserved, allocated, at, overhead, jobid, err4 = ReapiCliMatchAllocate(ctx, *reserve, string(jobspec))
        fmt.Printf("Match Allocate output:\n %s, %t, %d, %f, %s, %d, %s, %d, %d\n", allocated, reserved, at, overhead, err1, err2, err3, err4, jobid)

        err5 := ReapiCliCancel(ctx, 1, false)
        fmt.Printf("Cancel output: %d\n", err5)

        reserved2, at2, overhead2, err6 := ReapiCliInfo(ctx, 1)
        fmt.Printf("Info output jobid 1: %t, %d, %f, %d\n", reserved2, at2, overhead2, err6)

        reserved2, at2, overhead2, err6 = ReapiCliInfo(ctx, 2)
        fmt.Printf("Info output jobid 2: %t, %d, %f, %d\n", reserved2, at2, overhead2, err6)
}
