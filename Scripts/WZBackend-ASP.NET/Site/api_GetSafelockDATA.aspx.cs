using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;
using System.IO;

public partial class api_GetSafelockDATA : WOApiWebPage
{

    protected override void Execute()
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_GetSafelocksInfo";

        if (!CallWOApi(sqcmd))
            return;

        StringBuilder xml = new StringBuilder();
        xml.Append("<?xml version=\"1.0\"?>\n");

        while (reader.Read())
        {

            string Password = reader["Password"].ToString();
            string GamePos = reader["GamePos"].ToString();
            string GameRot = reader["GameRot"].ToString();
            string SafeLockID = reader["SafeLockID"].ToString();
            string ItemID = reader["ItemID"].ToString();
            string ExpireTime = reader["ExpireTime"].ToString();
            string MapID = reader["MapID"].ToString();
            string Quantity = reader["Quantity"].ToString();
            string Var1 = reader["Var1"].ToString();
            string Var2 = reader["Var2"].ToString();
            string GameServerID = reader["GameServerID"].ToString();
            string CustomerID = reader["CustomerID"].ToString();
            // string Durability = reader["Durability"].ToString();

            xml.Append("<SafeLock_items ");
            xml.Append(xml_attr("SafeLockID", SafeLockID));
            xml.Append(xml_attr("Password", Password));
            xml.Append(xml_attr("ItemID", ItemID));
            xml.Append(xml_attr("ExpireTime", ExpireTime));
            xml.Append(xml_attr("GamePos", GamePos));
            xml.Append(xml_attr("GameRot", GameRot));
            xml.Append(xml_attr("MapID", MapID));
            xml.Append(xml_attr("Quantity", Quantity));
            xml.Append(xml_attr("Var1", Var1));
            xml.Append(xml_attr("Var2", Var2));
            xml.Append(xml_attr("GameServerID", GameServerID));
            xml.Append(xml_attr("CustomerID", CustomerID));
            //   xml.Append(xml_attr("Durability", Durability));
            xml.Append("/>\n");
        }

        GResponse.Write(xml.ToString());

    }
}
