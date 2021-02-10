package main

/*
//gcc -Wall --machine-64 -c win/image_capture.cpp -o win/image_capture.o  -lwsock32
//ar -rv win/libimage_capture.a win/image_capture.o
#cgo CFLAGS: -I./win
#cgo windows LDFLAGS: -L./win -limage_capture -ld3d11 -lstdc++
//#cgo windows,amd64 LDFLAGS: -L${SRCDIR}/cdeps/win64 -lz
#include "win/image_capture.h"
*/
import "C"
import (
	"errors"
	"fmt"
	"time"
	"unsafe"
)

type Capture struct {
	mNwidth  int
	mNheight int
	data     []uint8
	dataSize int
}

func DXGI_InitCapture() {
	C.DXGI_InitCapture()
}
func DXGI_GetCapture() (*Capture, error) {
	var capture *Capture
	result := C.DXGI_GetCapture()
	fmt.Println(111)
	if result != nil {
		capture = new(Capture)
		fmt.Println(result)
		fmt.Println(result.m_nWidth, result.m_nHeight, result.data_size)
		capture.mNwidth = int(result.m_nWidth)
		capture.mNheight = int(result.m_nHeight)
		capture.dataSize = int(result.data_size)
		fmt.Println(capture.mNwidth, capture.mNheight, capture.dataSize)
		//start_addr := (uint8)(result.data)
		//fmt.Println("start_addr", start_addr)
		//sAddr := unsafe.Pointer(&start_addr)
		//data := (*[int(result.data_size)]uint8)(sAddr)
		capture.data = make([]uint8, capture.dataSize)
		C.memcpy(unsafe.Pointer(&(capture.data[0])), unsafe.Pointer(result.data), C.size_t(capture.dataSize))
		fmt.Println("data:", len(capture.data))
	}
	return capture, errors.New("image captrue is nil")
}
func DXGI_StopCapture() {
	C.DXGI_StopCapture()
}
func DXGI_ReleaseCapture() {
	C.DXGI_ReleaseCapture()
}

func main() {
	DXGI_InitCapture()
	for i := 0; i < 20; i++ {
		capture, err := DXGI_GetCapture()
		if err == nil {
			//capture := CCaptureToCapture(ccapture)
			fmt.Println("height:", capture.mNheight)
		}
		time.Sleep(time.Duration(20) * time.Millisecond)
	}
	fmt.Println("1")
	DXGI_StopCapture()
	DXGI_ReleaseCapture()
}
