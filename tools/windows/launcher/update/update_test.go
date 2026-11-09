// SPDX-License-Identifier: GPL-3.0-or-later

package update

import (
	"context"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"testing"
	"time"
)

const sample = `[
  {"tag_name": "windows-v9.9.9-alpha", "html_url": "https://github.com/reallinusweiss/ricasso/releases/tag/windows-v9.9.9-alpha", "draft": true},
  {"tag_name": "windows-v0.2.1-alpha", "html_url": "https://github.com/reallinusweiss/ricasso/releases/tag/windows-v0.2.1-alpha", "draft": false},
  {"tag_name": "windows-v.0.2.0-alpha", "html_url": "https://github.com/reallinusweiss/ricasso/releases/tag/windows-v.0.2.0-alpha", "draft": false},
  {"tag_name": "notes", "html_url": "https://example.com/", "draft": false}
]`

func TestNewestSkipsDraftsAndOddTags(t *testing.T) {
	r, err := Newest([]byte(sample))
	if err != nil {
		t.Fatal(err)
	}
	if r.Version != "0.2.1" || r.Tag != "windows-v0.2.1-alpha" {
		t.Fatalf("got %+v", r)
	}
	if r.URL != "https://github.com/reallinusweiss/ricasso/releases/tag/windows-v0.2.1-alpha" {
		t.Fatalf("url %q", r.URL)
	}
	if _, err := Newest([]byte(`[]`)); err == nil {
		t.Fatal("empty list should fail")
	}
}

func TestNewer(t *testing.T) {
	cases := []struct {
		cand, cur string
		want      bool
	}{
		{"0.3.0", "0.2.2", true},
		{"0.2.10", "0.2.9", true},
		{"0.2.2", "0.2.2", false},
		{"0.2.1", "0.3.0", false},
		{"1.0.0", "0.9.9", true},
		{"", "0.2.2", false},
	}
	for _, c := range cases {
		if got := Newer(c.cand, c.cur); got != c.want {
			t.Errorf("Newer(%q, %q) = %v", c.cand, c.cur, got)
		}
	}
}

func TestSafeURL(t *testing.T) {
	if SafeURL("https://evil.example/releases/") != ReleasesPage {
		t.Fatal("foreign URL accepted")
	}
	if SafeURL("https://github.com/reallinusweiss/ricasso/releases/tag/x") == ReleasesPage {
		t.Fatal("project URL rejected")
	}
}

func TestFetchSendsUserAgent(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Header.Get("User-Agent") != "Ricasso-Starter/test" {
			http.Error(w, "no UA", http.StatusForbidden)
			return
		}
		w.Write([]byte(sample))
	}))
	defer srv.Close()
	r, err := Fetch(context.Background(), srv.Client(), srv.URL, "Ricasso-Starter/test")
	if err != nil || r.Version != "0.2.1" {
		t.Fatalf("got %+v, %v", r, err)
	}
	if _, err := Fetch(context.Background(), srv.Client(), srv.URL, "other"); err == nil {
		t.Fatal("HTTP error not reported")
	}
}

func TestCacheRoundTrip(t *testing.T) {
	p := filepath.Join(t.TempDir(), "update-check.json")
	now := time.Now()
	if LoadCache(p).Fresh(now, 24*time.Hour) {
		t.Fatal("missing cache counts as fresh")
	}
	want := Cache{Checked: now, Latest: Release{Version: "0.3.0", Tag: "t", URL: ReleasesPage}}
	if err := SaveCache(p, want); err != nil {
		t.Fatal(err)
	}
	got := LoadCache(p)
	if got.Latest != want.Latest || !got.Fresh(now.Add(time.Hour), 24*time.Hour) {
		t.Fatalf("got %+v", got)
	}
	if got.Fresh(now.Add(25*time.Hour), 24*time.Hour) {
		t.Fatal("stale cache counts as fresh")
	}
	if got.Fresh(now.Add(-time.Hour), 24*time.Hour) {
		t.Fatal("cache from the future counts as fresh")
	}
}
