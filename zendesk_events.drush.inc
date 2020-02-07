<?php
/**
 * Implements hook_drush_command().
 */
 function zendesk_events_drush_command(){
     $items['calendar-events-zendesk-reference'] = array(
       'description' => 'Reference events in Google Cal to new Zendesk tickets.',
       'aliases' => array('cezr'),
       'drush calendar-events-zendesk-reference' => 'Triggers the internal note and tags for ZenDesk',
     );
     $items['zd-fetch'] = array(
      'description' => 'Fetches Zendesk Events.',
      'aliases' => array('zdf'),
      'drush zd-fetch' => 'Triggers the internal note and tags for ZenDesk'
    );
    return $items;
 }

 /**
  * Drush command logic.
  * drush_[MODULE_NAME]_[COMMAND_NAME]().
  */

 require_once DRUPAL_ROOT . '/sites/all/modules/custom/zendesk_events/vendor/autoload.php';
 use Zendesk\API\HttpClient as ZendeskAPI;

 // Initalizes the ZD Client
 function zendesk_client_init() {

     $subdomain = variable_get('zendesk_events_zd_api_subdomain', FALSE);
     $username = variable_get('zendesk_events_zd_api_user', FALSE);
     $token = variable_get('zendesk_events_zd_api_token', FALSE);

     if (!$subdomain || !$username || !$token) {
       throw new \Exception("Fatal: This script requires you to configure a Zendesk API endpoint and user. Please do this by setting the 'zendesk_events_zd_api_subdomain', 'zendesk_events_zd_api_user', and 'zendesk_events_zd_api_token' variables.");
     }
     else {
     $client = new ZendeskAPI($subdomain);
     $client->setAuth('basic', ['username' => $username, 'token' => $token]);
     return $client;
     }
 }
 // zendesk_client_init();

 //Take the TICKET IDs from Google Calendar and grab their ORG IDs from Zendesk.
function zendesk_events_orgs_from_gcal() {
   try {
     $client = zendesk_client_init();
     $orgs_from_gcal = array();
     // Grab the current calendar_events.module generated TICKET IDs from cache.
     $data = cache_get('cached_gcal_data');
     if (empty($data) == FALSE) {
       // Get Org IDs from tickets in the view.
       $seed_tickets = array_values($data->data);
       foreach ($seed_tickets as $ticket) {
         $zd_cal_ticket_find = $client->tickets()->find($ticket);
         $org_id = $zd_cal_ticket_find->ticket->organization_id;
         array_push($orgs_from_gcal, $org_id);
       }
     }
   }
   catch (ApiResponseException $zd_e) {
     watchdog_exception($zd_e);
     drush_print($zd_e);
   }
   return $orgs_from_gcal;
 }
 /**
  * Add internal comments or just expire what's currently in the view.
  */
function zendesk_events_add_comment() {
   $client = zendesk_client_init();
   $gcal_orgs = zendesk_events_orgs_from_gcal();
   // Return all tickets from our custom Zendesk view.
   try {
     $tickets_from_zd_view = $client->views(360068822093)->tickets();
    } catch (ApiResponseException $view_e) {
      watchdog_exception($view_e);
      drush_print($view_e);
    }
    // Grab TICKET IDs for each ticket, put them in an array, then print result.
    $zd_ticket_ids = array_column($tickets_from_zd_view->tickets, 'id');
    // Add some logging.
    drush_print("New Tickets to the queue:");
    if (!empty($zd_ticket_ids)) {
      drush_print_r($zd_ticket_ids);
    }
    else {
      drush_print("No new tickets to the queue.");
    }
    // Grab ORG IDs for each ticket, put them in an array, then print result.
    $zd_ticket_orgs = array_column($tickets_from_zd_view->tickets, 'organization_id');
    // Find ORG ID matches between Google Calendar and the Zendesk view.
    $matching_org_ids = array_intersect($zd_ticket_orgs, $gcal_orgs);
    if (!empty($matching_org_ids)) {
      // For tickets that have matches, Send update & tag ticket as event:yes.
      foreach ($tickets_from_zd_view->tickets as $ticket) {
        $array = array();
        $zd_cal_ticket_find = $ticket->id;
        $org_id = $ticket->organization_id;
        array_push($array, $zd_cal_ticket_find);
        if (in_array($org_id, $gcal_orgs)) {
          try {
         $client->tickets()->updateMany([
           'ids'    => $array,
           'comment'  => [
             'body' => "**Automated Ticket Notification**
                                     This customer may be having a High Traffic event right now. Please check the `Customers` calendar for more information.",
                         // Eventually add original ticket details here
                         // Associated Event Notification ticket: #$ticket->id
                         // Title of Associated Ticket: $ticket->subject,.
             'public' => FALSE,
           ],
           'additional_tags' => ['event:yes'],
         ]);
       } catch (ApiResponseException $match_e) {
          watchdog_exception($match_e);
          drush_print($match_e);
        }
        drush_print("There are event/ticket matches. Tickets with Event Matches:");
        drush_print_r($array);
       }
     }
   }
   if (!empty($zd_ticket_ids)) {
     // No match, set event:no tag.
     try {
       $client->tickets()->updateMany([
         'ids' => $zd_ticket_ids,
         'additional_tags' => ['event:no'],
       ]);
     } catch (ApiResponseException $no_match_e) {
       watchdog_exception($no_match_e);
       drush_print($no_match_e);
     }
   }
   return $matching_org_ids;
 }

 /**
  * Master function: `drush zd-command` will run in this function.
  */
  function drush_zendesk_events_calendar_events_zendesk_reference() {
    zendesk_events_add_comment();
    drush_print("Zendesk match and comment function has completed.");
  }

 function event_tickets_from_zendesk() {
    $client = zendesk_client_init();
    # how long, in seconds, should these cached objects last?
    $cache_lifetime = 3600;
    # establish the current time as a unix timestamp.
    $current_time = time();
    // WILL NEED TO ADD NEW VIEW WITH TICKET TYPE EVENT
    $tickets_from_events_view = $client->views(360068822093)->tickets();
    $new_events_notification_array = array();
    $tickets_to_close = array();

    foreach($tickets_from_events_view->tickets as $ticket) {
        $ticket_info = array();
        $id = $ticket->id;
        array_push($tickets_to_close, $id);
        $org_id = $ticket->organization_id;
        $subject =  $ticket->subject;
        $description = $ticket->description;

        foreach($ticket->custom_fields as $custom_field){
          if ($custom_field->id == 360016075373) {
             $start_date = $custom_field->value;
          } else if ($custom_field->id == 360016075393) {
             $end_date = $custom_field->value;
          } continue;
        }
        array_push($ticket_info, $id, $subject, $description, $start_date, $end_date);
        array_push($new_events_notification_array, $ticket_info);
    }

    cache_set('cached_event_tickets', $new_events_notification_array, 'cache', $current_time + $cache_lifetime);

// UPDATE THE ZENDESK TICKETS TO AUTOCLOSE AFTER BEING ADDED TO THE CALENDAR

    try {
        // print_r($tickets_from_events_view);
        $updatedTickets = $client->tickets()->updateMany(
            [
              'ids'    => $tickets_to_close,
              'assignee_id' => 370668600233,
              'status' => 'solved',
              // Find out what else is required for close
            ]
        );
        // print_r($updatedTickets->job_status);
        }
    catch (\Zendesk\API\Exceptions\ApiResponseException $e) {
    echo "API Response (event notification autosolve): ".$e->getMessage().'</br>';
    }
  }

// event_tickets_from_zendesk();

function drush_zendesk_events_zd_fetch()
{
    //GET Zendesk Tickets with Event Type
    event_tickets_from_zendesk();
    drush_print("zendesk fetch function run");
 }
