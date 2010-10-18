import os, numpy
import CdiObj

def htmlSkel(experiment):
  return ['<html> <head> <title>ECHAM Experiment: '+ experiment +'</title> <script type="text/javascript" src="http://code.jquery.com/jquery-1.4.2.min.js"></script> </head>' +
  """<body>
    <script>
      $(function() {
        $(\'tr.parent\')
        .css("cursor","pointer")
        .attr("title","Click to expand/collapse")
        .click(function(){
          $(this).siblings(\'.child-\'+this.id).toggle();
        });
        $(\'tr[@class^=child-]\').hide().children(\'td\');
        });</script>""",
  '''</body>
    </html>
  ''']

def tableSkel():
  return ["<table id=\"detail_table\" class=\"detail\"> <!--       <col style=\"width: 40px;\"> <col style=\"width: 80px;\"> <col style=\"width: 80px;\"> <col style=\"width: 80px;\">-->" +
          "<thead align=\"left\"><tr align=\"left\"><th>Code</th><th>North</th><th>South</th><th colspan=\"1000\">Global</th></tr></thead>",
          '</table>']

def tableRow(code,northVal,southVal,globalVal):
  return ["<tr  align=\"right\" title=\"Click to expand/collapse\" style=\"cursor: pointer;\" class=\"parent\" id=\"row%d\" >" % code +
          "<td valign=\"top\">%d</td><td>%3.3f</td><td>%3.3f</td><td>%3.3f</td>" % (code,northVal, southVal, globalVal) +
          '</tr>' + 
          "<tr style=\"display: none;\" class=\"child-row%d\">" % code + 
          '<td></td><td colspan="3">', 
          '</td></tr></table>']

def generateSubTable(headers, rows):
  html = []

  if len(headers) > 0:
    html.append("<tr>")
  for header in headers:
    html.append("<th>" + header + "</th>")
  html.append("</tr>")

  if len(rows) > 0:
    for row in rows:
      html.append("<tr>")
      for cell in row:
        html.append("<td>%3.3f</td>" % cell)
  html.append("</tr>")

  if html:
    html  = ["<table>"] + html + ["</table>"]

  return "".join(html)  

sourcefile  = "BOT.grb"
zonalfile   = "zonavg-BOT.grb"
cdoOperator = "zonavg"
cmd         = ' '.join(['cdo',cdoOperator, sourcefile, zonalfile])

print cmd

os.popen(cmd).read()

cdi = CdiObj.Cdi(zonalfile)

html = []
htmlStart, htmlEnd = htmlSkel('tableTest')
tableStart, tableEnd = tableSkel()

html.append(htmlStart)
html.append(tableStart)

for code in cdi.codes:

    var = cdi.varByCode[code]
    var.getValues()
    vals = numpy.array(var.values)
    lats = numpy.array(var.grid.yvalues)
    northInd = numpy.where(lats >= 0)
    southInd = numpy.where(lats <  0)
    print 'Global Mean:     ',vals.mean()
    print 'Global Max|Min:  ',vals.max(),'|',vals.min()
    print 'North  Mean:     ',vals[northInd].mean()
    print 'South  Mean:     ',vals[southInd].mean()
    
    codeTableStart, codeTableEnd = tableRow(code,vals[northInd].mean(), vals[southInd].mean(), vals.mean())
    
    html.append(codeTableStart)

    tdata = numpy.array([lats[northInd],vals[northInd],lats[southInd],vals[southInd]]).transpose().tolist()
    vtable  = generateSubTable(["latitude","values","latidute","values"],tdata)

    html.append(vtable)
#    html.append(codeTableEnd)


html.append(tableEnd)
html.append(htmlEnd)

f = open('testTable.html', 'w')
f.write("".join(html))
f.close
