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

public partial class api_ChangeName : WOApiWebPage
{
    void OutfitOp(string CustomerID, string Name, string CharID)
    {
        int cus = Convert.ToInt32(CustomerID);
        int CharI = Convert.ToInt32(CharID);

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_CharChangeName";
        sqcmd.Parameters.AddWithValue("@in_CharID", CharI);
        sqcmd.Parameters.AddWithValue("@in_CustomerID", cus);
        sqcmd.Parameters.AddWithValue("@in_Gamertag", Name);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");
    }

    protected override void Execute()
    {
        if (!WoCheckLoginSession())
            return;

        //         string func = web.Param("func");
        string CustomerID = web.Param("CustomerID");
        string Name = web.Param("Name");
        string CharID = web.Param("CharID");

        string rexCharValidate = @"^[a-zA-Z0-9]+$";
        if (Name.IndexOfAny("!@#$%^&*()-=+_<>,./?'\":;|{}[]".ToCharArray()) >= 0) // do not allow this symbols
        {
            Response.Write("WO_7");
            Response.Write("Character name cannot contain special symbols");
            return;
        }

        if (!Regex.IsMatch(Name, rexCharValidate))
        {
            // Email is not Valid
            Response.Write("WO_7");
            Response.Write("Character name cannot contain special symbols");
            return;
        }

        OutfitOp(CustomerID, Name, CharID);
    }
}