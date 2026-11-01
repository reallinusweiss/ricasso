//go:build testhash

// SPDX-License-Identifier: GPL-3.0-or-later

package importer

// Only for automated UI tests (go build -tags testhash): accepts an
// artificial test library as version 1.4.13. Never use in release builds.
func init() {
	KnownLibraries["af159a0c4ee4feda91a8107265e613e68dd4707a9635422b4c855f7a262bb0fb"] = "1.4.13"
}
