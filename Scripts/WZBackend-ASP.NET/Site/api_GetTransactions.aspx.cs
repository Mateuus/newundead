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

public partial class api_GetTransactions : WOApiWebPage
{
    protected override void Execute()
    {
        //if (!WoCheckLoginSession())
        //    return;

        string CustomerID = web.Param("CustomerID");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_GetTransactions";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);

        if (!CallWOApi(sqcmd))
            return;

        //reader.Read();

        // report page of leaderboard
        StringBuilder xml = new StringBuilder();
        xml.Append("<?xml version=\"1.0\"?>\n");
        xml.Append("<trans>");

        //reader.NextResult();
        while (reader.Read())
        {
            xml.Append("<data ");
            xml.Append(xml_attr("Time", reader["DateTime"]));
            xml.Append(xml_attr("TId", reader["TransactionID"]));
            xml.Append(xml_attr("ItemID", reader["ItemID"]));
            xml.Append(xml_attr("amount", reader["Amount"]));
            xml.Append(xml_attr("bl", reader["Balance"]));
            xml.Append(xml_attr("id", reader["id"]));
            xml.Append(" />");
        }
        xml.Append("</trans>");

        Response.Write(xml.ToString());
    }
}