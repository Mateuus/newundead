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

public partial class api_SrvGroupAdd : WOApiWebPage
{
    protected override void Execute()
    {

        string CustomerID = web.CustomerID();

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_BanPlr";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }
}