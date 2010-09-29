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

def tableSkel(code,northVal,southVal,globalVal):
   return ["<table id=\"detail_table\" class=\"detail\"> <!--       <col style=\"width: 40px;\"> <col style=\"width: 80px;\"> <col style=\"width: 80px;\"> <col style=\"width: 80px;\">-->" +
       "<thead align=\"left\"><tr align=\"right\"><th>Code</th><th>North</th><th>South</th><th>Global</th></tr></thead>" +
       "<tbody>" +
          "<tr  align=\"right\" title=\"Click to expand/collapse\" style=\"cursor: pointer;\" class=\"parent\" id=\"row%d\" >" % code +
          "<td  rowspan=\"2\" valign=\"top\">%d</td><td>%3.3f</td><td>%3.3f</td><td>%3.3f</td>" % (code,northVal, southVal, globalVal) +
          '</tr>' + 
          "<tr style=\"display: none;\" class=\"child-row%d\">" % code + 
          '<td colspan="3">', 
          '</td></tr></table>']

def generate_html_table(headers, rows):
  html = []

  if len(headers) > 0:
    html.append("<tr>")
  for header in headers:
    html.append("<th>%3.3f</th>" % header)
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

html.append(htmlStart)

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
  
  codeTableStart, codeTableEnd = tableSkel(code,vals[northInd].mean(), vals[southInd].mean(), vals.mean())
  
  html.append(codeTableStart)

  vtable  = generate_html_table(lats[southInd],[vals[southInd]])
  vtable += generate_html_table(lats[northInd],[vals[northInd]])

  html.append(vtable)
  html.append(codeTableEnd)

html.append(htmlEnd)

f = open('testTable.html', 'w')
f.write("".join(html))
f.close
