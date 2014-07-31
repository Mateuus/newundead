using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;

public partial class api_WGLoginSession : WOApiWebPage
{
    protected override void Execute()
    {
        string key = web.Param("key");
        string CustomerID = web.Param("CustomerID");
       
        int key1 = Convert.ToInt32(key);
         if (key1 != 54871231)
        {
            Response.Write("KEY INVAILD");
            return;
        }

         SqlCommand sqcmd = new SqlCommand();
         sqcmd.CommandType = CommandType.StoredProcedure;
         sqcmd.CommandText = "WG_SetLoginSession";
         sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);

         if (!CallWOApi(sqcmd))
             return;
         Response.Write("WO_0");
    }
}
