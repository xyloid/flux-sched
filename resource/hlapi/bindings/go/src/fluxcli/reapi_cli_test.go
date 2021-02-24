package fluxcli

import "testing"

func TestFluxcli(t *testing.T) {
	ctx := NewReapiCli()
	if ctx == nil {
		t.Errorf("Context is null")
	}

}
