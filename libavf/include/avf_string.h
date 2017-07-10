
#ifndef __AVF_STRING_H__
#define __AVF_STRING_H__

//-----------------------------------------------------------------------
//
// auto pointer
//
//-----------------------------------------------------------------------
template <typename T>
class ap
{
public:
	INLINE ap(): m_ptr(0) {}

	INLINE ap(T* other)
		: m_ptr(other)
	{
		if (other)
			other->AddRef();
	}

	INLINE ap(const ap<T>& other)
		: m_ptr(other.m_ptr)
	{
		if (m_ptr)
			m_ptr->AddRef();
	}

	INLINE void clear() {
		if (m_ptr) {
			m_ptr->Release();
			m_ptr = 0;
		}
	}

	INLINE ~ap() {
		if (m_ptr)
			m_ptr->Release();
	}

	// assignment
	INLINE ap<T>& operator=(T* other) {
		if (other) other->AddRef();
		if (m_ptr) m_ptr->Release();
		m_ptr = other;
		return *this;
	}
	INLINE ap<T>& operator=(const ap<T>& other) {
		if (other.m_ptr) other.m_ptr->AddRef();
		if (m_ptr) m_ptr->Release();
		m_ptr = other.m_ptr;
		return *this;
	}

	// accessors
	INLINE T& operator*() const { return *m_ptr; }
	INLINE T* operator->() const { return m_ptr; }
	INLINE T* get() const { return m_ptr; }

private:
	T* m_ptr;
};

//-----------------------------------------------------------------------
//
// string
//
//-----------------------------------------------------------------------
struct string_t
{
protected:
	avf_atomic_t mRefCount;
	avf_size_t mSize;	// not including the '\0'
	// follows string array
	void DoRelease();

public:
	static string_t *CreateFrom(const char *string, avf_size_t len);
	INLINE static string_t *CreateFrom(const char *string) {
		return CreateFrom(string, ::strlen(string));
	}
	static string_t *Add(const char *a, avf_size_t size_a,
		const char *b, avf_size_t size_b,
		const char *c, avf_size_t size_c,
		const char *d, avf_size_t size_d);

	INLINE void AddRef() {
		avf_atomic_inc(&mRefCount);
	}

	INLINE void Release() {
		if (avf_atomic_dec(&mRefCount) == 1) {
			DoRelease();
		}
	}

	INLINE const char *string() {
		return (const char*)(this + 1);
	}

	INLINE avf_size_t size() {
		return mSize;
	}

	INLINE string_t* acquire() {
		AddRef();
		return this;
	}
};

template<>
class ap<string_t>
{
public:
	// constructors
	INLINE ap() {
		m_ptr = string_t::CreateFrom("", 0);
	}

	INLINE ap(const char *other) {
		m_ptr = string_t::CreateFrom(other, ::strlen(other));
	}

	INLINE ap(const char *other, avf_size_t len) {
		m_ptr = string_t::CreateFrom(other, len);
	}

	INLINE ap(string_t *other) : m_ptr(other) {
		m_ptr->AddRef();
	}

	INLINE ap(const ap<string_t>& other) : m_ptr(other.m_ptr) {
		m_ptr->AddRef();
	}

	INLINE ap(const char *a, const char *b) {
		m_ptr = string_t::Add(a, ::strlen(a), b, ::strlen(b),
			NULL, 0, NULL, 0);
	}

	INLINE ap(const char *a, const char *b, const char *c) {
		m_ptr = string_t::Add(a, ::strlen(a), b, ::strlen(b),
			c, ::strlen(c), NULL, 0);
	}

	INLINE ap(string_t *a, const char *b) {
		m_ptr = string_t::Add(a->string(), a->size(), b, ::strlen(b),
			NULL, 0, NULL, 0);
	}

	INLINE ap(string_t *a, const char *b, const char *c) {
		m_ptr = string_t::Add(a->string(), a->size(), b, ::strlen(b),
			c, ::strlen(c), NULL, 0);
	}

	INLINE ap(string_t *a, string_t *b) {
		m_ptr = string_t::Add(a->string(), a->size(), b->string(), b->size(),
			NULL, 0, NULL, 0);
	}

	INLINE ap(string_t *a, string_t *b, const char *c) {
		m_ptr = string_t::Add(a->string(), a->size(), b->string(), b->size(),
			c, ::strlen(c), NULL, 0);
	}

	INLINE ap(string_t *a, string_t *b, const char *c, const char *d) {
		m_ptr = string_t::Add(a->string(), a->size(), b->string(), b->size(),
			c, ::strlen(c), d, ::strlen(d));
	}

	INLINE ap(string_t *a, string_t *b, string_t *c) {
		m_ptr = string_t::Add(a->string(), a->size(), b->string(), b->size(),
			c->string(), c->size(), NULL, 0);
	}

	INLINE ap(string_t *a, string_t *b, string_t *c, string_t *d) {
		m_ptr = string_t::Add(a->string(), a->size(), b->string(), b->size(),
			c->string(), c->size(), d->string(), d->size());
	}

	INLINE ap(string_t *a, string_t *b, string_t *c, const char *d) {
		m_ptr = string_t::Add(a->string(), a->size(), b->string(), b->size(),
			c->string(), c->size(), d, ::strlen(d));
	}

	INLINE ap(string_t *a, const char *b, string_t *c, const char *d) {
		m_ptr = string_t::Add(a->string(), a->size(), b, ::strlen(b),
			c->string(), c->size(), d, ::strlen(d));
	}

	// destructor
	INLINE ~ap() {
		m_ptr->Release();
	}

	void clear();

	// =
	ap<string_t>& operator=(const char* other);
	ap<string_t>& operator=(string_t* other);
	ap<string_t>& operator=(const ap<string_t>& other);

	// +=
	ap<string_t>& operator+=(const char *other);

	INLINE ap<string_t>& operator+=(string_t* other) {
		Append(other);
		return *this;
	}

	INLINE ap<string_t>& operator+=(const ap<string_t>& other) {
		Append(other.m_ptr);
		return *this;
	}

	// accessors
	INLINE string_t& operator*() const { return *m_ptr; }
	INLINE string_t* operator->() const { return m_ptr; }
	INLINE string_t* get() const { return m_ptr; }

	INLINE bool operator==(const char *other) {
		return ::strcmp(m_ptr->string(), other) == 0;
	}

private:
	void Append(string_t *other);

	INLINE void SetTo(const char *other) {
		string_t *pNew = string_t::CreateFrom(other, ::strlen(other));
		m_ptr->Release();
		m_ptr = pNew;
	}

	INLINE void SetTo(const ap<string_t>& other) {
		other.m_ptr->AddRef();
		m_ptr->Release();
		m_ptr = other.m_ptr;
	}

private:
	// always points to a valid object (default is g_null_string)
	string_t *m_ptr;
};

#endif

