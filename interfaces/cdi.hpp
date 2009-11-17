#include <vector>
#include <map>
#define CHARSIZE      128

class CdiGrid {
  public:
    CdiGrid();
    CdiGrid(int gridid);
    ~CdiGrid();

    int gridID;
    int type, size, xsize, ysize, prec, ncorner;
    bool hasBounds;
    double *xvals   , *yvals;
    float  *xvalsF  , *yvalsF;
    double *xbounds , *ybounds;
    float  *xboundsF, *yboundsF;

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
    double *levels, *lbounds, *ubounds, *weights;
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
    double *field;
    double **fieldWithLevel;
    float  *fieldF;
    float  **fieldWithLevelF;


    CdiGrid  grid;
    CdiZaxis zaxis;
    CdiTaxis taxis;

    void print();
    void readField(); 
    void readFieldWithLevel(int tsID = 0);
    void readFieldF();
    void readFieldWithLevelF(int tsID = 0);
    std::vector<double> getField();
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
