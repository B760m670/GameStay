package main

// Привязка к игровому ядру (C++) через cgo: те же правила, что на телефоне.
// Перед сборкой: ./build_core.sh (собирает engine/build-server/liblf_server.a).

/*
#cgo CFLAGS: -I${SRCDIR}/../engine/src/capi
#cgo LDFLAGS: -L${SRCDIR}/../engine/build-server -llf_server -lstdc++ -lm
#include <stdlib.h>
#include "lf_capi.h"
*/
import "C"

import (
	"errors"
	"unsafe"
)

func goStr(p *C.char) string {
	if p == nil {
		return ""
	}
	s := C.GoString(p)
	C.lf_free(unsafe.Pointer(p))
	return s
}

func cstr(s string) (*C.char, func()) {
	c := C.CString(s)
	return c, func() { C.free(unsafe.Pointer(c)) }
}

// NewVillage — сохранение новой деревни.
func NewVillage(nowMs int64) string { return goStr(C.lf_new_village(C.longlong(nowMs))) }

// TickSave проверяет сохранение и доводит его до текущего момента.
func TickSave(save string, nowMs int64) (string, error) {
	c, free := cstr(save)
	defer free()
	out := C.lf_tick_save(c, C.longlong(nowMs))
	if out == nil {
		return "", errors.New("bad save")
	}
	return goStr(out), nil
}

// Summary — уровень, рейтинг, уровень острога, ресурсы из сохранения.
func Summary(save string) (map[string]int64, error) {
	c, free := cstr(save)
	defer free()
	out := C.lf_summary(c)
	if out == nil {
		return nil, errors.New("bad save")
	}
	return parseKV(goStr(out)), nil
}

// Snapshot — база игрока для атаки с долей добычи.
func Snapshot(save string, lootFrac float64) (string, error) {
	c, free := cstr(save)
	defer free()
	out := C.lf_snapshot(c, C.double(lootFrac))
	if out == nil {
		return "", errors.New("bad save")
	}
	return goStr(out), nil
}

// VerifyAttack прогоняет запись боя на сервере и возвращает итог и новое сохранение атакующего.
func VerifyAttack(attackerSave, snapshot, replay string, seed int, nowMs int64) (map[string]int64, string, error) {
	a, f1 := cstr(attackerSave)
	defer f1()
	s, f2 := cstr(snapshot)
	defer f2()
	r, f3 := cstr(replay)
	defer f3()
	var outSave, errStr *C.char
	res := C.lf_verify_attack(a, s, r, C.int(seed), C.longlong(nowMs), &outSave, &errStr)
	if res == nil {
		return nil, "", errors.New(goStr(errStr))
	}
	return parseKV(goStr(res)), goStr(outSave), nil
}

// ApplyDefense снимает добычу с защитника и меняет его рейтинг.
func ApplyDefense(defenderSave string, ore, aether int64, stars int, win bool) (string, int, error) {
	c, free := cstr(defenderSave)
	defer free()
	w := 0
	if win {
		w = 1
	}
	var delta C.int
	out := C.lf_apply_defense(c, C.longlong(ore), C.longlong(aether), C.int(stars), C.int(w), &delta)
	if out == nil {
		return "", 0, errors.New("bad defender save")
	}
	return goStr(out), int(delta), nil
}

// AutoReplay — честная запись боя (для тестов и ботов).
func AutoReplay(attackerSave, snapshot string, seed int, nowMs int64) string {
	a, f1 := cstr(attackerSave)
	defer f1()
	s, f2 := cstr(snapshot)
	defer f2()
	return goStr(C.lf_auto_replay(a, s, C.int(seed), C.longlong(nowMs)))
}
