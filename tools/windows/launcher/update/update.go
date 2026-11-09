// SPDX-License-Identifier: GPL-3.0-or-later

// Package update asks GitHub whether a newer Ricasso release exists. It only
// reads the public release list; nothing is downloaded or installed.
package update

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"
	"regexp"
	"strconv"
	"strings"
	"time"
)

const (
	// ReleasesAPI lists releases including pre-releases (/releases/latest
	// skips pre-releases, and every Ricasso release so far is one).
	ReleasesAPI = "https://api.github.com/repos/reallinusweiss/ricasso/releases?per_page=30"
	// ReleasesPage is opened in the browser when no release URL is known.
	ReleasesPage = "https://github.com/reallinusweiss/ricasso/releases"
)

// Release is the newest published Ricasso release.
type Release struct {
	Version string `json:"version"` // e.g. "0.3.0"
	Tag     string `json:"tag"`
	URL     string `json:"url"`
}

var versionRe = regexp.MustCompile(`(\d+)\.(\d+)\.(\d+)`)

// parse extracts major.minor.patch from a version or tag such as
// "windows-v0.2.1-alpha" or "windows-v.0.2.0-alpha".
func parse(s string) ([3]int, bool) {
	m := versionRe.FindStringSubmatch(s)
	if m == nil {
		return [3]int{}, false
	}
	var v [3]int
	for i := range v {
		n, err := strconv.Atoi(m[i+1])
		if err != nil {
			return [3]int{}, false
		}
		v[i] = n
	}
	return v, true
}

func compare(a, b [3]int) int {
	for i := range a {
		if a[i] != b[i] {
			if a[i] < b[i] {
				return -1
			}
			return 1
		}
	}
	return 0
}

// Newer reports whether candidate is a higher version than current.
func Newer(candidate, current string) bool {
	c, ok1 := parse(candidate)
	cur, ok2 := parse(current)
	return ok1 && ok2 && compare(c, cur) > 0
}

// SafeURL returns u if it points to this project's releases on GitHub,
// otherwise the general release page.
func SafeURL(u string) string {
	if strings.HasPrefix(u, "https://github.com/reallinusweiss/ricasso/releases/") {
		return u
	}
	return ReleasesPage
}

type ghRelease struct {
	TagName string `json:"tag_name"`
	HTMLURL string `json:"html_url"`
	Draft   bool   `json:"draft"`
}

// Newest picks the highest version from a GitHub release list, skipping drafts.
func Newest(body []byte) (Release, error) {
	var list []ghRelease
	if err := json.Unmarshal(body, &list); err != nil {
		return Release{}, err
	}
	var best Release
	var bestV [3]int
	for _, r := range list {
		v, ok := parse(r.TagName)
		if r.Draft || !ok {
			continue
		}
		if best.Version == "" || compare(v, bestV) > 0 {
			bestV = v
			best = Release{Version: fmt.Sprintf("%d.%d.%d", v[0], v[1], v[2]), Tag: r.TagName, URL: SafeURL(r.HTMLURL)}
		}
	}
	if best.Version == "" {
		return Release{}, errors.New("no published release found")
	}
	return best, nil
}

// Fetch downloads the release list from url and returns the newest release.
func Fetch(ctx context.Context, client *http.Client, url, userAgent string) (Release, error) {
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
	if err != nil {
		return Release{}, err
	}
	req.Header.Set("Accept", "application/vnd.github+json")
	req.Header.Set("User-Agent", userAgent)
	resp, err := client.Do(req)
	if err != nil {
		return Release{}, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return Release{}, fmt.Errorf("GitHub answered %s", resp.Status)
	}
	body, err := io.ReadAll(io.LimitReader(resp.Body, 4<<20))
	if err != nil {
		return Release{}, err
	}
	return Newest(body)
}

// Cache keeps the last answer so GitHub is asked at most once a day.
type Cache struct {
	Checked time.Time `json:"checked"`
	Latest  Release   `json:"latest"`
}

// Fresh reports whether the cached answer is younger than maxAge.
func (c Cache) Fresh(now time.Time, maxAge time.Duration) bool {
	age := now.Sub(c.Checked)
	return !c.Checked.IsZero() && age >= 0 && age < maxAge
}

// LoadCache reads the cache file; a missing or broken file gives an empty cache.
func LoadCache(path string) Cache {
	var c Cache
	if data, err := os.ReadFile(path); err == nil {
		if json.Unmarshal(data, &c) != nil {
			c = Cache{}
		}
	}
	return c
}

// SaveCache writes the cache file.
func SaveCache(path string, c Cache) error {
	data, err := json.MarshalIndent(c, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(path, append(data, '\n'), 0o644)
}
