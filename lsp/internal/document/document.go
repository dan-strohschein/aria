// Package document manages open file overlays for the LSP server.
package document

import "sync"

type Document struct {
	URI     string
	Content string
	Version int32
}

type Store struct {
	mu   sync.RWMutex
	docs map[string]*Document
}

func NewStore() *Store {
	return &Store{docs: make(map[string]*Document)}
}

func (s *Store) Open(uri, content string, version int32) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.docs[uri] = &Document{URI: uri, Content: content, Version: version}
}

func (s *Store) Update(uri, content string, version int32) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if doc, ok := s.docs[uri]; ok {
		doc.Content = content
		doc.Version = version
	}
}

func (s *Store) Close(uri string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	delete(s.docs, uri)
}

func (s *Store) Get(uri string) (*Document, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	doc, ok := s.docs[uri]
	return doc, ok
}

func (s *Store) All() []*Document {
	s.mu.RLock()
	defer s.mu.RUnlock()
	result := make([]*Document, 0, len(s.docs))
	for _, doc := range s.docs {
		result = append(result, doc)
	}
	return result
}
