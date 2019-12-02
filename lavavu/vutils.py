"""
LavaVu python interface: vis utils
"""
import sys
import os

def is_ipython():
    """
    Detects if running within IPython environment

    Returns
    -------
    boolean
        True if IPython detected
        Does not necessarrily indicate running within a browser / notebook context
    """
    try:
        if __IPYTHON__:
            return True
        else:
            return False
    except:
        return False

def is_notebook():
    """
    Detects if running within an interactive IPython notebook environment

    Returns
    -------
    boolean
        True if IPython detected and browser/notebook display capability detected
    """
    if 'IPython' not in sys.modules:
        # IPython hasn't been imported, definitely not
        return False
    try:
        from IPython import get_ipython
        from IPython.display import display,Image,HTML
    except:
        return False
    # check for `kernel` attribute on the IPython instance
    return getattr(get_ipython(), 'kernel', None) is not None

def getname(var):
    """
    Attempt to find the name of a variable from the main module namespace

    Parameters
    ----------
    var
        The variable in question

    Returns
    -------
    name : str
        Name of the variable
    """
    import __main__ as main_mod
    for name in dir(main_mod):
        val = getattr(main_mod, name)
        if val is var:
            return name
    return None

def download(url, filename=None, overwrite=False, quiet=False):
    """
    Download a file from an internet URL

    Parameters
    ----------
    url : str
        URL to request the file from
    filename : str
        Filename to save, default is to keep the same name in current directory
    overwrite : boolean
        Always overwrite file if it exists, default is to never overwrite

    Returns
    -------
    filename : str
        Actual filename written to local filesystem
    """
    #Python 3 moved modules
    try:
        from urllib.request import urlopen, URLError, HTTPError, Request
        from urllib.parse import urlparse
        from urllib.parse import quote
    except ImportError:
        from urllib2 import urlopen, URLError, HTTPError, Request
        from urllib import quote
        from urlparse import urlparse

    if filename is None:
        filename = url[url.rfind("/")+1:]

    if overwrite or not os.path.exists(filename):
        #Encode url path
        o = urlparse(url)
        o = o._replace(path=quote(o.path))
        url = o.geturl()
        if not quiet: print("Downloading: " + filename)
        try:
            #Use a fake user agent, as some websites disallow python/urllib
            user_agent = 'Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.0.7) Gecko/2009021910 Firefox/3.0.7'
            headers = {'User-Agent':user_agent,}
            request = Request(url, None, headers)
            response = urlopen(request)
            with open(filename, "wb") as out:
                out.write(response.read())
        except (Exception) as e:
            print(str(e), url)
            raise(e)

    else:
        if not quiet:
            print(filename + " exists, skipped downloading.")

    return filename

def inject(html):
    """
    Inject HTML into a notebook cell
    """
    if not is_notebook():
        return
    from IPython.display import display,HTML
    display(HTML(html))

def injectjs(js):
    """
    Inject Javascript into a notebook cell
    """
    inject('<script>' + js + '</script>')

def hidecode():
    """
    Allow toggle of code cells
    """
    inject('''<script>
    var code_show = '';
    var menu_edited = false;
    function code_toggle() {
      var code_cells = document.getElementsByClassName('CodeMirror');
      if (code_cells.length == 0) return;

      if (code_show == 'none')
        code_show = '';
      else
        code_show = 'none';

      for (var x=0; x<code_cells.length; x++) {
        var el = code_cells[x].parentElement;
        el.style.display = code_show;

        //Show cell code on double click
        var p = el.parentElement.parentElement;
        p.ondblclick = function() {
          var pel = this.parentElement;
          var all = pel.getElementsByTagName('*');
          for (var i = 0; i < all.length; i++) {
            if (all[i].classList.contains('CodeMirror')) {
              all[i].parentElement.style.display = '';
              break;
            }
          }
        }
      }
    }

    if (!menu_edited) {
      menu_edited = true;
      var mnu = document.getElementById("view_menu");
      console.log(mnu)
      if (mnu) {
        var element = document.createElement('li');
        element.id = 'toggle_toolbar';
        element.title = 'Show/Hide code cells'
        element.innerHTML = "<a href='javascript:code_toggle()'>Toggle Code Cells</a>"
        mnu.appendChild(element);
      }
    }
    code_toggle();
    </script>
    The code for this notebook is hidden <a href="#" onclick="code_toggle()">Click here</a> to show/hide code.''')

def style(css):
    """
    Inject stylesheet
    """
    inject("<style>" + css + "</style>")

def cellstyle(css):
    """
    Override the notebook cell styles
    """
    style("""
    div.container {{
        {css}
    }}
    """.format(css=css))

def cellwidth(width='99%'):
    """
    Override the notebook cell width
    """
    cellstyle("""
    width:{width} !important;
    margin-left:1%;
    margin-right:auto;
    """.format(width=width))


