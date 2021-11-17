package main

import (
	"flag"
	. "fluxcli"
	"fmt"
	"io/ioutil"
)

func main() {
	ctx := NewReapiCli()
	jgfPtr := flag.String("jgf", "", "path to jgf")
	jobspecPtr := flag.String("jobspec", "", "path to jobspec")
	reserve := flag.Bool("reserve", false, "or else reserve?")
	flag.Parse()

	jgf, err := ioutil.ReadFile(*jgfPtr)
	if err != nil {
		fmt.Println("Error reading JGF file")
		return
	}
	_, fluxerr = ReapiCliInit(ctx, string(jgf))
	if err != nil {
		fmt.Println("Error init ReapiCli")
		return
	}

	jobspec, err := ioutil.ReadFile(*jobspecPtr)
	if err != nil {
		fmt.Println("Error reading jobspec file")
		return
	}
	fmt.Printf("Jobspec:\n %s\n", jobspec)

	reserved, allocated, at, pre, post, overhead, jobid, fluxerr := ReapiCliMatchAllocate(ctx, *reserve, string(jobspec))
	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliMatchAllocate")
		return
	}
	printOutput(reserved, allocated, at, pre, post, overhead, jobid, fluxerr)
	reserved, allocated, at, pre, post, overhead, jobid, fluxerr = ReapiCliMatchAllocate(ctx, *reserve, string(jobspec))
	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliMatchAllocate")
		return
	}
	printOutput(reserved, allocated, at, pre, post, overhead, jobid, fluxerr)
	fluxerr = ReapiCliCancel(ctx, 1, false)
	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliCancel")
		return
	}
	fmt.Printf("Cancel output: %d\n", fluxerr)

	reserved, at, overhead, fluxerr = ReapiCliInfo(ctx, 1)
	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliInfo")
		return
	}
	fmt.Printf("Info output jobid 1: %t, %d, %f, %d\n", reserved, at, overhead, fluxerr)

	reserved, at, overhead, fluxerr = ReapiCliInfo(ctx, 2)
	if fluxerr != 0 {
		fmt.Println("Error in ReapiCliInfo")
		return
	}
	fmt.Printf("Info output jobid 2: %t, %d, %f, %d\n", reserved, at, overhead, fluxerr)

}

func printOutput(reserved bool, allocated string, at int64, pre uint32, post uint32, overhead float64, jobid uint64, fluxerr int) {
	fmt.Println("\n\t----Match Allocate output---")
	fmt.Printf("jobid: %d\nreserved: %t\nallocated: %s\nat: %d\npreorder visit count: %d\npostorder visit count: %d\noverhead: %f\nerror: %d\n", jobid, reserved, allocated, at, pre, post, overhead, fluxerr)
}
