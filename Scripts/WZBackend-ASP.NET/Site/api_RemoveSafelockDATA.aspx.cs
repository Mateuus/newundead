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
using System.Text.RegularExpressions;

public partial class api_RemoveSafelockDATA : WOApiWebPage
{
    void OutfitOp(string SafeLockID, string ItemID, string Quantity, string Var1, string Var2, string GameServerID)
    {
        int SafeI = Convert.ToInt32(SafeLockID);
        int ItemI = Convert.ToInt32(ItemID);
        int QuantityI = Convert.ToInt32(Quantity);
        int Var1i = Convert.ToInt32(Var1);
        int Var2i = Convert.ToInt32(Var2);
        int GameServerIDI = Convert.ToInt32(GameServerID);

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SAFELOCK_DELETE";
        sqcmd.Parameters.AddWithValue("@in_SafeLockID", SafeI);
        sqcmd.Parameters.AddWithValue("@in_ItemID", ItemI);
        sqcmd.Parameters.AddWithValue("@in_Quantity", QuantityI);
        sqcmd.Parameters.AddWithValue("@in_Var1", Var1i);
        sqcmd.Parameters.AddWithValue("@in_Var2", Var2i);
        sqcmd.Parameters.AddWithValue("@in_GameServerID", GameServerIDI);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        if (!WoCheckLoginSession())
            return;

      //string func = web.Param("func");
        string SafeLockID = web.Param("SafeLockID");
        string ItemID = web.Param("ItemID");
        string Quantity = web.Param("Quantity");
        string Var1 = web.Param("Var1");
        string Var2 = web.Param("Var2");
        string GameServerID = web.Param("GameServerID");

        OutfitOp(SafeLockID, ItemID, Quantity, Var1, Var2, GameServerID);
    }
}