using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;
using System.Configuration;

public partial class api_AccCheckKey : WOApiWebPage
{
    protected override void Execute()
    {
        Response.Write("WO_0");
        Response.Write("0");
    }
}
