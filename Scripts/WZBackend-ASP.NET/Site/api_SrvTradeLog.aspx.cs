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

public partial class api_SrvTradeLog : WOApiWebPage
{
    protected override void Execute()
    {
        //if (!WoCheckLoginSession())
        //   return;
        string CustomerID = web.Param("CustomerID");
        string CharID = web.Param("CharID");
        string Gamertag = web.Param("Gamertag");
        string ItemID = web.Param("ItemID");
        string quantity = web.Param("quantity");
        string GameServerId = web.Param("GameServerId");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SRV_TradeLog";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_CharID", CharID);
        sqcmd.Parameters.AddWithValue("@in_Gamertag", Gamertag);
        sqcmd.Parameters.AddWithValue("@in_ItemID", ItemID);
        sqcmd.Parameters.AddWithValue("@in_quantity", quantity);
        sqcmd.Parameters.AddWithValue("@in_GameServerId", GameServerId);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }
}