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

public partial class Web_webapi_AccountChangePass : WOApiWebPage
{
    protected override void Execute()
    {
        string CustomerID = web.CustomerID();
        string SessionID = web.SessionKey();
        string Password = web.Param("password");
        string NewPassword = web.Param("newpassword");
        string ConfirmPassword = web.Param("confirmpassword");
        string rexPasswordValidate = @"^[a-zA-Z0-9]+$";

        if (!Regex.IsMatch(Password, rexPasswordValidate) || !Regex.IsMatch(NewPassword, rexPasswordValidate) || !Regex.IsMatch(ConfirmPassword, rexPasswordValidate))
        {
            Response.Write("WO_4");
            Response.Write("Password is not Valid");
            return;
        }
        else if (Password.Length < 4 || NewPassword.Length < 4 || ConfirmPassword.Length < 4)
        {
            Response.Write("WO_5");
            Response.Write("Password Size must over 4 character");
            return;
        }
        else if (NewPassword != ConfirmPassword)
        {
            Response.Write("WO_6");
            Response.Write("Password missmatch");
            return;
        }

        SaveApiLog("webapi_AccountChangePass.aspx");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_Account_ChangePassword";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_SessionID", SessionID);
        sqcmd.Parameters.AddWithValue("@in_Password", Password);
        sqcmd.Parameters.AddWithValue("@in_NewPassword", NewPassword);

        if (!CallWOApi(sqcmd))
            return;

        Response.Write("WO_0");

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
