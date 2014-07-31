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

public partial class api_Login : WOApiWebPage
{
    protected override void Execute()
    {
        string username = web.Param("userfuckingshit");
        string password = web.Param("passwordfuckyou");
        string serverkey = web.Param("serverkey");
        string computerid = web.Param("computerid");
        string miniacrc = web.Param("miniacrc");
        string warguardcrc = web.Param("warguardcrc");

       // GResponse.Write("WO_0");
       // return;

        //SaveApiLog("api_Login.aspx");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WZ_ACCOUNT_LOGIN";
        sqcmd.Parameters.AddWithValue("@in_IP", LastIP);
        sqcmd.Parameters.AddWithValue("@in_EMail", username);
        sqcmd.Parameters.AddWithValue("@in_Password", password);
        sqcmd.Parameters.AddWithValue("@in_HardwareID", computerid);
        sqcmd.Parameters.AddWithValue("@in_MiniACRC", miniacrc);
        sqcmd.Parameters.AddWithValue("@in_WarGuardCRC", warguardcrc);
      
      
       
        if (!CallWOApi(sqcmd))
            return;

        reader.Read();
        int CustomerID = getInt("CustomerID"); ;
        int AccountStatus = getInt("AccountStatus");
        int SessionID = 0;
        int IsDeveloper = 0;
        //int IsModerator = 0;

        if (CustomerID > 0)
        {
            SessionID = getInt("SessionID");
            IsDeveloper = getInt("IsDeveloper");
          //  IsModerator = getInt("IsModerator");
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
