INTERFACE:

class Fpu_state;

class Fpu_state_ptr
{
public:
  bool valid() const;
  Fpu_state *get() const;
  void set(Fpu_state *state);

  Fpu_state_ptr();
  ~Fpu_state_ptr();

private:
  friend class Fpu_alloc;

  Fpu_state *_state;
};

IMPLEMENTATION:

IMPLEMENT inline
Fpu_state_ptr::Fpu_state_ptr() : _state(nullptr)
{}

IMPLEMENT inline
Fpu_state_ptr::~Fpu_state_ptr()
{
}

IMPLEMENT inline
bool Fpu_state_ptr::valid() const
{
  return _state != nullptr;
}

IMPLEMENT inline
Fpu_state *Fpu_state_ptr::get() const
{
  return _state;
}

IMPLEMENT inline
void Fpu_state_ptr::set(Fpu_state *state)
{
  _state = state;
}
