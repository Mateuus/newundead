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

public partial class api_ChangePassSafeLock : WOApiWebPage
{
    void OutfitOp(string SafeLockID, string Password, string GameServerID)
    {
        int SafeI = Convert.ToInt32(SafeLockID);
        int GameServerIDI = Convert.ToInt32(GameServerID);

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_SRV_ChangepassSafeLock";
        sqcmd.Parameters.AddWithValue("@in_SafeLockID", SafeI);
        sqcmd.Parameters.AddWithValue("@in_Password", Password);
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
        string Password = web.Param("Password");
        string GameServerID = web.Param("GameServerID");

         if (Password.IndexOfAny("!@#$%^&*()-=+_<>,./?'\":;|{}[]".ToCharArray()) >= 0) // do not allow this symbols
         {
             Response.Write("WO_7");
             Response.Write("the Password cannot contain special symbols");
             return;
         }

        OutfitOp(SafeLockID, Password, GameServerID);
    }
}