package fluxmodule

import ( "testing"
         "io/ioutil"
	 "fmt"
)

func TestFluxmodule(t *testing.T) {
	ctx := NewReapiModule()
	jobspec, err := ioutil.ReadFile("/root/flux-sched/t/data/resource/jobspecs/basics/test001.yaml")
	if ctx == nil {
		t.Errorf("Context is null")
	}
	reserved, allocated, at, overhead, err1 := ReapiModuleMatchAllocate(ctx, false, string(jobspec), 4)
	fmt.Printf("%t, %s, %d, %f, %d, %s", reserved, allocated, at, overhead, err1, err)

}
