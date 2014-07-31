using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Text;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Data.SqlClient;
using System.Text.RegularExpressions;

public partial class api_CharSlots : WOApiWebPage
{
    void CharRevive()
    {
        string CustomerID = web.CustomerID();
        string CharID = web.Param("CharID");
        string Health = web.Param("Health");

        //@TODO: buy revive item?
        {
        }

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_CharRevive";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_CharID", CharID);
        sqcmd.Parameters.AddWithValue("@in_Health", Health);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    void CharCreate()
    {
        string rexCharValidate = @"^[a-zA-Z0-9]+$";
        string Gamertag = web.Param("Gamertag");
        if(Gamertag.IndexOfAny("!@#$%^&*()-=+_<>,./?'\":;|{}[]".ToCharArray()) >= 0) // do not allow this symbols
        {
            Response.Write("WO_7");
            Response.Write("Character name cannot contain special symbols");
            return;
        }

        if (!Regex.IsMatch(Gamertag, rexCharValidate))
        {
            // Email is not Valid
            Response.Write("WO_7");
            Response.Write("Character name cannot contain special symbols");
            return;
        }

        string CustomerID = web.CustomerID();

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_CharCreate";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_Hardcore", web.Param("Hardcore"));
        sqcmd.Parameters.AddWithValue("@in_Gamertag", Gamertag);
        sqcmd.Parameters.AddWithValue("@in_HeroItemID", web.Param("HeroItemID"));
        sqcmd.Parameters.AddWithValue("@in_HeadIdx", web.Param("HeadIdx"));
        sqcmd.Parameters.AddWithValue("@in_BodyIdx", web.Param("BodyIdx"));
        sqcmd.Parameters.AddWithValue("@in_LegsIdx", web.Param("LegsIdx"));

        if (!CallWOApi(sqcmd))
            return;

        reader.Read();
        int CharID = getInt("CharID");

        Response.Write("WO_0");
        Response.Write(CharID.ToString());
    }

    void CharDelete()
    {
        string CustomerID = web.CustomerID();
        string CharID = web.Param("CharID");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_CharDelete";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_CharID", CharID);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        if (!WoCheckLoginSession())
            return;

        string func = web.Param("func");
        if (func == "revive")
            CharRevive();
        else if (func == "create")
            CharCreate();
        else if (func == "delete")
            CharDelete();
        else
            throw new ApiExitException("bad func");
    }
}
