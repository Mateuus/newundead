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

public partial class Web_webapi_AccountView : WOApiWebPage
{
    private string accountinformationJson;
    private string characterinformationJson;
    protected override void Execute()
    {
        //        string json = @"{
        //                      \"Email\": \"komkit0710@hotmail.com\",
        //                      \"CustomerID\": \"101101\",
        //                      \"AccountStatus\": \"100\",
        //                      \"GamePoints\": \"5000\",
        //                      \"GameDollars\": \"4000\",
        //                      \"FacebookPoints\": \"103.55.11.3038\",
        //                      \"LoginCount\": \"100\",
        //                      \"LastIP\": \"103.55.11.38\"
        //                    }";

        SaveApiLog("webapi_AccountView.aspx");

        getAccountInformation();
        getCharacterInfo();
        string json = "{" + String.Format("{0},\"Characters\":[{1}]", accountinformationJson, characterinformationJson) + "}";
        Response.Write(json);
    }

    private void getAccountInformation()
    {
        string CustomerID = web.CustomerID();
        string SessionID = web.SessionKey();


        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_Account_View";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_SessionID", SessionID);

        if (!CallWOApi(sqcmd))
            return;

        reader.Read();
        string Email = getString("Email");
        int aCustomerID = getInt("CustomerID");
        int AccountStatus = getInt("AccountStatus");
        int GamePoints = getInt("GamePoints");
        int GameDollars = getInt("GameDollars");
        int FacebookPoints = getInt("FacebookPoints");
        int LoginCount = getInt("LoginCount");
        string LastIP = getString("LastIP");

        accountinformationJson = String.Format("\"Email\":\"{0}\",\"CustomerID\":\"{1}\",\"AccountStatus\":\"{2}\",\"GamePoints\":\"{3}\",\"GameDollars\":\"{4}\",\"FacebookPoints\":\"{5}\",\"LoginCount\":\"{6}\",\"LastIP\":\"{7}\"", Email, aCustomerID, AccountStatus, GamePoints, GameDollars, FacebookPoints, LoginCount, LastIP);
    }

    private void getCharacterInfo()
    {
        string CustomerID = web.CustomerID();
        string SessionID = web.SessionKey();
        characterinformationJson = "";

        SqlCommand sqcmd = new SqlCommand();
        sqcmd.CommandType = CommandType.StoredProcedure;
        sqcmd.CommandText = "WEB_Character_View";
        sqcmd.Parameters.AddWithValue("@in_CustomerID", CustomerID);
        sqcmd.Parameters.AddWithValue("@in_SessionID", SessionID);

        if (!CallWOApi(sqcmd))
            return;

        while (reader.Read())
        {
            int aCustomerID = getInt("CustomerID");
            int CharID = getInt("CharID");
            string Gamertag = getString("Gamertag");
            string ClanName = getString("ClanName");

            characterinformationJson += "{" + String.Format("\"CustomerID\":\"{0}\",\"CharID\":\"{1}\",\"Gamertag\":\"{2}\",\"ClanName\":\"{3}\"", aCustomerID, CharID, Gamertag, ClanName) + "},";
        }
        characterinformationJson = characterinformationJson.TrimEnd(',');
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
