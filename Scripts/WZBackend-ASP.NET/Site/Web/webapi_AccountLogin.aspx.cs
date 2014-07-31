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

public partial class Web_webapi_AccountLogin : WOApiWebPage
{
    protected override void Execute()
    {
        string username = web.Param("username");
        string password = web.Param("password");
        string ip = web.Param("ip");

        string rexEmailValidate = @"\A(?:[a-zA-Z0-9!#$%&*+/=?^_`{|}~-]+(?:\.[a-zA-Z0-9!#$%&*+/=?^_`{|}~-]+)*@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?\.)+[a-zA-Z0-9](?:[a-zA-Z0-9-]*[a-zA-Z0-9])?)\Z";
        string rexPasswordValidate = @"^[a-zA-Z0-9]+$";

        if (!Regex.IsMatch(username, rexEmailValidate))
        {
            // Email is not Valid
            Response.Write("WO_3");
            Response.Write("Email is not Valid");
            return;
        }
        else if (username.Length < 4)
        {
            Response.Write("WO_3");
            Response.Write("Email is not Valid");
            return;
        }
        else if (password.Length < 4)
        {
            Response.Write("WO_4");
            Response.Write("Password is not Valid");
            return;
        }
        else if (!Regex.IsMatch(password, rexPasswordValidate))
        {
            Response.Write("WO_4");
            Response.Write("Password is not Valid");
            return;
        }

        SaveApiLog("webapi_AccountLogin.aspx");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_Account_Login";
        sqcmd.Parameters.AddWithValue("@in_IP", ip);
        sqcmd.Parameters.AddWithValue("@in_EMail", username);
        sqcmd.Parameters.AddWithValue("@in_Password", password);

        if (!CallWOApi(sqcmd))
            return;

        reader.Read();
        int CustomerID = getInt("CustomerID"); ;
        int AccountStatus = getInt("AccountStatus");
        int SessionID = 0;
        int IsDeveloper = 0;

        if (CustomerID > 0)
        {
            SessionID = getInt("SessionID");
            IsDeveloper = getInt("IsDeveloper");
        }

        GResponse.Write("WO_0");
        GResponse.Write(string.Format("{0} {1} {2}",
            CustomerID, SessionID, AccountStatus));
    }

    private void SaveApiLog(string apiname)
    {
        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_ApiLog";
        sqcmd.Parameters.AddWithValue("@in_IP", Request.UserHostAddress);
        sqcmd.Parameters.AddWithValue("@in_ApiName", apiname);
        sqcmd.Parameters.AddWithValue("@in_QueryString", Request.Url.Query);

        if (!CallWOApi(sqcmd))
            return;
    }
}
