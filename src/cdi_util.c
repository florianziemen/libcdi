
void cdiDecodeParam(int param, int *pnum, int *pcat, int *pdis)
{
  int iparam;

  iparam = param;
  if ( param < 0 ) iparam = -iparam;
  *pdis  = iparam / 1000000;
  iparam = iparam - *pdis*1000000;
  *pcat = iparam / 1000;
  *pnum = iparam - *pcat*1000;
  if ( param < 0 ) *pnum = -(*pnum);
}


int cdiEncodeParam(int pnum, int pcat, int pdis)
{
  int param;
  int inum;

  if ( pcat < 0 || pcat > 255 ) pcat = 255;
  if ( pdis < 0 || pdis > 255 ) pdis = 255;

  inum = pnum;
  if ( pnum < 0 ) inum = -pnum;
  param = pdis*1000000 + pcat*1000 + inum;
  if ( pnum < 0 ) param = -param;

  return (param);
}


void cdiDecodeDate(int date, int *year, int *month, int *day)
{
  int idate;

  *year  =  date / 10000;
  idate  = date - *year*10000;
  if ( idate < 0 ) idate = -idate;
  *month = idate / 100;
  *day   = idate - *month*100;
}


int cdiEncodeDate(int year, int month, int day)
{
  int date;
  int iyear;

  iyear = year;
  if ( iyear < 0 ) iyear = -iyear;
  date = iyear*10000 + month*100 + day;
  if ( year < 0 ) date = -date;

  return (date);
}


void cdiDecodeTime(int time, int *hour, int *minute, int *second)
{
  int itime;

  *hour   = time / 10000;
  itime   = time - *hour*10000;
  *minute = itime / 100;
  *second = itime - *minute*100;
}


int cdiEncodeTime(int hour, int minute, int second)
{
  int time;

  time = hour*10000 + minute*100 + second;

  return (time);
}
