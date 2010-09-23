#include <vector>
#include <map>
#define CHARSIZE      128

using namespace std;

class CdiGrid {
  public:
    CdiGrid();
    CdiGrid(int gridid);
    ~CdiGrid();

    int gridID;
    int type, size, xsize, ysize, prec, ncorner;
    bool hasBounds;
    double *xvals , *yvals , *xbnds , *ybnds;
    float  *xvalsF, *yvalsF, *xbndsF, *ybndsF;
    std::vector<double> xvalues, yvalues, xbounds, ybounds;

    std::string xname, xlongname, xstdname, xunits;
    std::string yname, ylongname, ystdname, yunits;      
    std::string name;

    void readFloatBounds();
    void readFloatVals();
      
  private:
    void determineGrid(int gridID);
};

class CdiTaxis {
  public:
    CdiTaxis();
    CdiTaxis(int vlistID);
    ~CdiTaxis();
    
    int taxisID;
    int ntsteps, unit;
    int rdate, rtime, vdate, vtime;
    int type, calendar, hasBounds;
    char name[CHARSIZE], *unitname;
};

class CdiZaxis {
  public:
    CdiZaxis();
    CdiZaxis(int zaxisid);
    ~CdiZaxis();

    int zaxisID;
    int type, ltype, size, prec;
    double *plevels, *plbounds, *pubounds, *pweights;
    vector<double> levels, lbounds, ubounds, weights;
    std::string name, longname, units;
};

class CdiVariable { 
  public:
    CdiVariable();
    CdiVariable(int streamid,int vlistid, int varid);
    ~CdiVariable();

    int    varID, zaxisID, gridID, taxisID, timeID, vlistID;
    int    size, code, datatype;
    int    streamID;
    std::string  name, longname, units, stdname;
    double missval;
            vector<double>   values;
    vector< vector<double> > valuesWithLevel;


    CdiGrid  grid;
    CdiZaxis zaxis;
    CdiTaxis taxis;

    void sinfo();
    void getValues(); 
    void getValuesWithLevel(int tsID = 0);
            vector<float>   getFValues();
    vector< vector<float> > getFValuesWithLevel(int tsID = 0);
                   double  *getValuesAsPointer();
                   double **getValuesWithLevelAsPointer(int tsID = 0);
};

class Cdi {
  public:
    Cdi(const char *path);
    ~Cdi();
    int streamID;
    int vlistID,nvars,nzaxes,ngrids,ntaxes,taxisID;

    std::vector <std::string> varnames;
    std::vector <int> codes;
    
    std::vector<CdiVariable>           variables;
    std::map<std::string, CdiVariable> var;
    std::map<int        , CdiTaxis>    taxes;
    std::map<int        , CdiZaxis>    zaxes;
    std::map<int        , CdiGrid>     grids;
  private:
    void getTaxes();
    void getZaxes();
    void getGrids();
    void getVars();
};
