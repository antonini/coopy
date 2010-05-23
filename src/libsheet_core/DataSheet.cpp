
#include <coopy/DataSheet.h>

std::string DataSheet::encode(const SheetStyle& style) const {
  std::string delim = style.getDelimiter();
  std::string result = "";
  for (int y=0;y<height();y++) {
    for (int x=0;x<width();x++) {
      if (x>0) {
	result += delim;
      }
      result += encodeCell(cellString(x,y),style);
    }
    result += "\r\n"; // use Windows encoding, since UNIX is more forgiving
  }
  return result;
}

std::string DataSheet::encodeCell(const std::string& str, 
				  const SheetStyle& style) {
  std::string delim = style.getDelimiter();
  bool need_quote = false;
  for (int i=0; i<str.length(); i++) {
    char ch = str[i];
    if (ch=='"'||ch=='\''||ch==delim[0]||ch=='\r'||ch=='\n'||ch=='\t'||ch==' ') {
      need_quote = true;
      break;
    }
  }
  std::string result = "";
  if (need_quote) { result += '"'; }
  for (int i=0; i<str.length(); i++) {
    char ch = str[i];
    if (ch=='"') {
      result += '"';
    }
    result += ch;
  }
  if (need_quote) { result += '"'; }
  return result;
}