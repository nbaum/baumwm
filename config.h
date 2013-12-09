
const int BorderWidth = 5;
const int HeadlineHeight = 20;
const int ModMask = Mod4Mask;

inline unsigned long rgb (unsigned char blue, unsigned char green, unsigned char red)
{
  return red | (green << 8) | (blue << 16);
}

inline unsigned long rgb (unsigned char gray)
{
  return gray | (gray << 8) | (gray << 16);
}

