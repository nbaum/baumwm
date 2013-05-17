#ifndef util_h
#define util_h

inline void parse_modifiers (const char *&spec, int &mask)
{
  mask = 0;
  for (; spec[1] == '-'; spec += 2) {
    switch (spec[0]) {
    case 'S':
      mask |= ShiftMask;
      break;
    case 'C':
      mask |= ControlMask;
      break;
    case 'A':
      mask |= Mod1Mask;
      break;
    case 'M':
      mask |= Mod2Mask;
      break;
    }
  }
}

inline void parse_key (const char *spec, int &sym, int &mask)
{
  parse_modifiers(spec, mask);
  sym = XStringToKeysym(spec);
}

inline void parse_button (const char *spec, int &button, int &mask)
{
  parse_modifiers(spec, mask);
  button = atoi(spec);
}

#endif