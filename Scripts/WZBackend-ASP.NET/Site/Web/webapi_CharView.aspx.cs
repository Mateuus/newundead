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

public partial class Web_webapi_CharView : WOApiWebPage
{
    private string characterinformationJson;

    protected override void Execute()
    {
        Response.Write(characterinformationJson);
    }

    private void getCharacterInfo()
    {
        string CustomerID = web.CustomerID();
        string SessionID = web.SessionKey();
        characterinformationJson = "";

        SaveApiLog("webapi_CharView.aspx");

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_Character_View";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        //sqcmd.Parameters.AddWithValue("@in_SessionID", SessionID);

        if (!CallWOApi(sqcmd))
            return;

        while (reader.Read())
        {
            int aCustomerID = getInt("CustomerID");
            int CharID = getInt("CharID");
            string Gamertag = getString("Gamertag");

            characterinformationJson += "{" + String.Format("\"CustomerID\":\"{0}\",\"CharID\":\"{1}\",\"Gamertag\":\"{2}\"},", aCustomerID, CharID, Gamertag);
        }
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
