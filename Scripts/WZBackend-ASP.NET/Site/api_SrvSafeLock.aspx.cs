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

public partial class api_SrvSafeLock : WOApiWebPage
{
    void OutfitOp(string SafeLockID, string Password, string ItemID, string ExpireTime, string GamePos, string GameRot, string MapID, string Quantity, string Var1, string Var2, string Category, string GameServerID, string CustomerID/*, string Durability*/)
    {
        int SafeI = Convert.ToInt32(SafeLockID);
        int ItemI = Convert.ToInt32(ItemID);
        int MapI = Convert.ToInt32(MapID);
        int ExpireTimeI = Convert.ToInt32(ExpireTime);
        int Quantity1 = Convert.ToInt32(Quantity);
        int Vars1 = Convert.ToInt32(Var1);
        int Vars2 = Convert.ToInt32(Var2);
        int GameServerIDI = Convert.ToInt32(GameServerID);
        int CustomerID_1 = Convert.ToInt32(CustomerID);
        // int Durability_1 = Convert.ToInt32(Durability);

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SRV_SafeLockADD";
        sqcmd.Parameters.AddWithValue("@in_SafeLockID", SafeI);
        sqcmd.Parameters.AddWithValue("@in_Password", Password);
        sqcmd.Parameters.AddWithValue("@in_ItemID", ItemI);
        sqcmd.Parameters.AddWithValue("@in_ExpireTime", ExpireTimeI);
        sqcmd.Parameters.AddWithValue("@in_GamePos", GamePos);
        sqcmd.Parameters.AddWithValue("@in_GameRot", GameRot);
        sqcmd.Parameters.AddWithValue("@in_MapID", MapI);
        sqcmd.Parameters.AddWithValue("@in_Quantity", Quantity1);
        sqcmd.Parameters.AddWithValue("@in_Var1", Vars1);
        sqcmd.Parameters.AddWithValue("@in_Var2", Vars2);
        sqcmd.Parameters.AddWithValue("@in_Category", Category);
        sqcmd.Parameters.AddWithValue("@in_GameServerID", GameServerIDI);
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID_1);
        //   sqcmd.Parameters.AddWithValue("@in_Durability", Durability_1);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        if (!WoCheckLoginSession())
            return;

        //         string func = web.Param("func");
        string SafeLockID = web.Param("SafeLockID");
        string Password = web.Param("Password");
        string ItemID = web.Param("ItemID");
        string ExpireTime = web.Param("ExpireTime");
        string GamePos = web.Param("GamePos");
        string GameRot = web.Param("GameRot");
        string MapID = web.Param("MapID");
        string Quantity = web.Param("Quantity");
        string Var1 = web.Param("Var1");
        string Var2 = web.Param("Var2");
        string Category = web.Param("Category");
        string GameServerID = web.Param("GameServerID");
        string CustomerID = web.Param("CustomerID");
      //string Durability = web.Param("Durability"); 

        if (Password.IndexOfAny("!@#$%^&*()-=+_<>,./?'\":;|{}[]".ToCharArray()) >= 0) // do not allow this symbols
        {
            Response.Write("WO_7");
            Response.Write("the Password cannot contain special symbols");
            return;
        }

        OutfitOp(SafeLockID, Password, ItemID, ExpireTime, GamePos, GameRot, MapID, Quantity, Var1, Var2, Category, GameServerID, CustomerID/*, Durability*/);
    }
}